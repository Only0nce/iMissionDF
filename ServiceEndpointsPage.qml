import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Item {
    id: root

    property string dfServerIp: ""
    property string appliedDfServerIp: ""
    property string compassOffsetText: ""
    property string statusText: ""
    property bool busy: false
    property string pendingDfServerIp: ""
    property bool dfEndpointApplyInProgress: false

    signal requestToast(string text)

    QtObject {
        id: c
        property color panel: "#132235"
        property color card: "#122033"
        property color field: "#0d1723"
        property color border: "#2d4056"
        property color borderSoft: "#203044"
        property color text: "#e9f0f7"
        property color sub: "#9aa8b8"
        property color muted: "#667589"
        property color accent: "#00c9a7"
        property color warning: "#f59e0b"
        property color info: "#38bdf8"
        property color danger: "#ef4444"
        property color purple: "#a855f7"
    }

    function kraken() {
        return (typeof Krakenmapval !== "undefined" && Krakenmapval) ? Krakenmapval : null
    }

    function closeKeyboard() {
        root.forceActiveFocus()
        Qt.inputMethod.hide()
    }

    function isValidIpv4(ip) {
        var parts = String(ip).trim().split(".")
        if (parts.length !== 4)
            return false
        for (var i = 0; i < 4; ++i) {
            if (!/^\d{1,3}$/.test(parts[i]))
                return false
            var n = Number(parts[i])
            if (!isFinite(n) || n < 0 || n > 255 || Math.floor(n) !== n)
                return false
        }
        return true
    }

    function requestLegacyState() {
        var km = kraken()
        if (!km) {
            statusText = "Service endpoint backend is unavailable"
            return
        }

        // Parameter.ipdfserver is authoritative for DF Server IP.  Ask C++ for
        // the DB-backed snapshot first; updateIPServerDF() is only a fast cache
        // replay and must not become the owner of this field.
        if (typeof km.requestDfServerEndpointSnapshot === "function")
            km.requestDfServerEndpointSnapshot()
        else if (typeof km.updateIPServerDF === "function")
            km.updateIPServerDF()

        if (typeof km.updateIPServerDF === "function")
            km.updateIPServerDF()

        // Keep the legacy Network2 refresh for LAN/global-offset state only.
        // Network2.krakenserver is a mirror/fallback and must never overwrite a
        // DF endpoint Apply that is in progress.
        if (typeof km.getNetworkfromDb === "function")
            km.getNetworkfromDb(1)
        else if (typeof km.requestNetworkRows === "function")
            km.requestNetworkRows()
    }

    function applyDfServer() {
        closeKeyboard()
        var ip = String(dfServerField.text).trim()
        if (!isValidIpv4(ip)) {
            statusText = "Invalid DF Server IPv4 address"
            requestToast(statusText)
            return
        }

        var km = kraken()
        if (!km || typeof km.connectToDFserver !== "function") {
            statusText = "DF Server backend is unavailable"
            requestToast(statusText)
            return
        }

        busy = true
        root.pendingDfServerIp = ip
        root.dfEndpointApplyInProgress = true
        statusText = "Applying DF Server IP and reconnecting..."

        // DF Server IP is separate from LAN3/end0 and from Network2 LAN rows.
        // Do not call the legacy Mainwindows network hook here; connectToDFserver()
        // owns Parameter.ipdfserver persistence and the TCP reconnect attempt.
        root.appliedDfServerIp = ip
        root.dfServerIp = ip
        dfServerField.text = ip

        km.connectToDFserver(ip)
        applyDoneTimer.restart()
    }

    function reconnectDfServer() {
        closeKeyboard()
        var km = kraken()
        if (!km || typeof km.connectToDFserver !== "function") {
            statusText = "DF Server backend is unavailable"
            requestToast(statusText)
            return
        }

        var savedIp = String(root.appliedDfServerIp || "").trim()
        if (!isValidIpv4(savedIp)) {
            statusText = "Saved DF Server IP is not available"
            requestToast(statusText)
            return
        }

        busy = true
        statusText = "Reconnecting DF Server " + savedIp + "..."

        // Reconnect to the last applied DF Server IP, not to an unsaved TextField
        // draft and not to LAN3/end0 configuration.
        km.connectToDFserver(savedIp)
        reconnectDoneTimer.restart()
    }

    function setCompassOffset() {
        closeKeyboard()
        var value = Number(String(compassField.text).trim())
        if (!isFinite(value)) {
            statusText = "Compass Offset must be a valid number"
            requestToast(statusText)
            return
        }

        var km = kraken()
        if (!km || typeof km.setCompassOffset !== "function") {
            statusText = "Compass Offset backend is unavailable"
            requestToast(statusText)
            return
        }

        busy = true
        statusText = "Saving Compass Offset..."
        km.setCompassOffset(value)
        compassDoneTimer.restart()
    }

    Component.onCompleted: requestLegacyState()

    Connections {
        target: root.kraken()
        ignoreUnknownSignals: true

        function onNetworkRowUpdated(row) {
            if (!row)
                return

            var serverIp = ""

            // This is the same Network2/krakenserver DB payload consumed by
            // the legacy TopNetworkDrawer. Prefer the selected row, otherwise
            // fall back to the first non-empty value from the all-row snapshot.
            if (row.krakenserver !== undefined && row.krakenserver !== null)
                serverIp = String(row.krakenserver).trim()

            if (serverIp.length === 0 && row.all) {
                try {
                    var obj = JSON.parse(row.all)
                    var rows = obj.rows || []
                    for (var i = 0; i < rows.length; ++i) {
                        var candidate = String(rows[i].krakenserver || "").trim()
                        if (candidate.length > 0) {
                            serverIp = candidate
                            break
                        }
                    }
                } catch (e) {
                    console.log("[ServiceEndpoints] invalid Network2 snapshot:", e)
                }
            }

            // Network2 is only a legacy fallback.  During/after Apply the UI is
            // sticky to the operator-selected DF endpoint; a delayed Network2
            // refresh must not roll the field back to a boot/default value.
            if (root.dfEndpointApplyInProgress)
                return

            if (serverIp.length > 0 && root.appliedDfServerIp.length === 0) {
                root.appliedDfServerIp = serverIp
                root.dfServerIp = serverIp
                if (!dfServerField.activeFocus)
                    dfServerField.text = serverIp
            }
        }

        function onUpdateServeripDfserver(ip) {
            var normalized = String(ip || "").trim()
            if (normalized.length === 0)
                return

            if (root.dfEndpointApplyInProgress &&
                    root.pendingDfServerIp.length > 0 &&
                    normalized !== root.pendingDfServerIp) {
                console.log("[ServiceEndpoints] ignore stale DF endpoint while apply is active:",
                            normalized, "pending=", root.pendingDfServerIp)
                return
            }

            root.appliedDfServerIp = normalized
            root.dfServerIp = normalized
            if (!dfServerField.activeFocus || normalized === root.pendingDfServerIp)
                dfServerField.text = normalized

            if (normalized === root.pendingDfServerIp) {
                root.pendingDfServerIp = ""
                root.dfEndpointApplyInProgress = false
            }
        }

        function onDfServerEndpointTransactionChanged(state, candidateIp, committedIp, detail) {
            var st = String(state || "")
            var committed = String(committedIp || "").trim()
            if (committed.length > 0) {
                if (!root.dfEndpointApplyInProgress ||
                        root.pendingDfServerIp.length === 0 ||
                        committed === root.pendingDfServerIp) {
                    root.appliedDfServerIp = committed
                    root.dfServerIp = committed
                    if (!dfServerField.activeFocus || committed === root.pendingDfServerIp)
                        dfServerField.text = committed
                }
            }

            if (root.pendingDfServerIp.length > 0 && committed === root.pendingDfServerIp &&
                    (st === "COMMITTED" || st === "CONNECTED" || st === "CONNECT_RETRY" ||
                     st === "DB_SAVE_FAILED" || st === "DB_SAVE_QUEUE_FAILED")) {
                root.pendingDfServerIp = ""
                root.dfEndpointApplyInProgress = false
            }

            if (detail && String(detail).length > 0)
                root.statusText = st + " — " + String(detail)
            else if (st.length > 0)
                root.statusText = st
        }

        function onUpdateGlobalOffsets(offsetValue, compassOffset) {
            var value = Number(compassOffset)
            root.compassOffsetText = isFinite(value) ? value.toFixed(6) : ""
            if (!compassField.activeFocus)
                compassField.text = root.compassOffsetText
        }
    }

    Timer {
        id: applyDoneTimer
        interval: 350
        repeat: false
        onTriggered: {
            root.busy = false
            root.statusText = "DF Server Apply requested"
        }
    }

    Timer {
        id: reconnectDoneTimer
        interval: 350
        repeat: false
        onTriggered: {
            root.busy = false
            root.statusText = "DF Server Reconnect requested"
        }
    }

    Timer {
        id: compassDoneTimer
        interval: 250
        repeat: false
        onTriggered: {
            root.busy = false
            root.statusText = "Compass Offset update requested"
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 30
        anchors.rightMargin: 30
        anchors.topMargin: 30
        anchors.bottomMargin: 70
        spacing: 24

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            radius: 18
            color: c.panel
            border.color: c.border

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 28
                spacing: 18

                Text {
                    text: "DF Server Endpoint"
                    color: c.text
                    font.pixelSize: 26
                    font.bold: true
                    Layout.fillWidth: true
                }

                Text {
                    text: "DF control/GPS connect to this RFSoC endpoint. DoA Viewer receives the live data through the local iScan bridge. LAN3 is configured separately."
                    color: c.sub
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: c.borderSoft
                }

                Text {
                    text: "DF Server IP"
                    color: c.sub
                    font.pixelSize: 13
                    font.bold: true
                }

                TextField {
                    id: dfServerField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 54
                    text: root.dfServerIp
                    placeholderText: "192.168.99.8"
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    color: c.text
                    font.pixelSize: 18
                    leftPadding: 14
                    rightPadding: 14
                    selectByMouse: true
                    background: Rectangle {
                        radius: 10
                        color: c.field
                        border.color: dfServerField.activeFocus ? c.accent : c.border
                        border.width: dfServerField.activeFocus ? 2 : 1
                    }
                }

                Text {
                    text: root.appliedDfServerIp.length > 0
                          ? "Saved endpoint: " + root.appliedDfServerIp + " · TCP 5555 · GPSD 2947"
                          : "Saved endpoint has not been loaded yet"
                    color: c.muted
                    font.pixelSize: 12
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    Button {
                        id: applyDfButton
                        text: "Apply"
                        Layout.preferredWidth: 170
                        Layout.preferredHeight: 48
                        enabled: !root.busy
                        scale: pressed ? 0.96 : 1.0
                        Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
                        onClicked: root.applyDfServer()
                        contentItem: Text { text: parent.text; color: parent.enabled ? "#001412" : c.sub; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 15; font.bold: true }
                        background: Rectangle { radius: 10; color: applyDfButton.enabled ? c.accent : c.field; border.color: applyDfButton.enabled ? c.accent : c.border }
                    }

                    Button {
                        id: reconnectDfButton
                        text: "Reconnect"
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 48
                        enabled: !root.busy && root.appliedDfServerIp.length > 0
                        scale: pressed ? 0.96 : 1.0
                        Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
                        onClicked: root.reconnectDfServer()
                        contentItem: Text { text: parent.text; color: parent.enabled ? c.text : c.sub; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 15; font.bold: true }
                        background: Rectangle { radius: 10; color: reconnectDfButton.pressed ? "#15384f" : c.field; border.color: reconnectDfButton.enabled ? c.info : c.border }
                    }

                    Item { Layout.fillWidth: true }
                }

                Item { Layout.fillHeight: true }

                Text {
                    text: "Viewer and Admin can both edit Service Endpoints."
                    color: c.info
                    font.pixelSize: 12
                    font.bold: true
                    Layout.fillWidth: true
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 1
            radius: 18
            color: c.panel
            border.color: c.border

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 28
                spacing: 18

                Text {
                    text: "Global Offsets"
                    color: c.text
                    font.pixelSize: 26
                    font.bold: true
                    Layout.fillWidth: true
                }

                Text {
                    text: "Apply a global heading correction used by compass/DoA presentation. The value is persisted in the existing parameter database."
                    color: c.sub
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: c.borderSoft
                }

                Text {
                    text: "Compass Offset"
                    color: c.sub
                    font.pixelSize: 13
                    font.bold: true
                }

                TextField {
                    id: compassField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 54
                    text: root.compassOffsetText
                    placeholderText: "0.000000"
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    color: c.text
                    font.pixelSize: 18
                    leftPadding: 14
                    rightPadding: 14
                    selectByMouse: true
                    background: Rectangle {
                        radius: 10
                        color: c.field
                        border.color: compassField.activeFocus ? c.purple : c.border
                        border.width: compassField.activeFocus ? 2 : 1
                    }
                }

                Text {
                    text: "Current saved offset: " + (root.compassOffsetText.length > 0 ? root.compassOffsetText : "—")
                    color: c.muted
                    font.pixelSize: 12
                    Layout.fillWidth: true
                }

                Button {
                    id: setCompassButton
                    text: "Set Compass Offset"
                    Layout.preferredWidth: 220
                    Layout.preferredHeight: 48
                    enabled: !root.busy
                    scale: pressed ? 0.96 : 1.0
                    Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
                    onClicked: root.setCompassOffset()
                    contentItem: Text { text: parent.text; color: "#ffffff"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 15; font.bold: true }
                    background: Rectangle { radius: 10; color: setCompassButton.pressed ? Qt.darker(c.purple, 1.18) : c.purple; border.color: c.purple }
                }

                Item { Layout.fillHeight: true }

                Text {
                    text: "Saved through the existing compass_offset parameter path."
                    color: c.info
                    font.pixelSize: 12
                    font.bold: true
                    Layout.fillWidth: true
                }
            }
        }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 16
        width: Math.min(parent.width - 120, 780)
        height: statusText.length > 0 ? 38 : 0
        visible: statusText.length > 0
        radius: 19
        color: "#0d1723"
        border.color: c.border
        Text { anchors.centerIn: parent; width: parent.width - 30; text: root.statusText; color: c.sub; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight }
    }
}
