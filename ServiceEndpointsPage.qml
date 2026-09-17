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

        // NET-ENDPOINTS1.5: DatabaseDF already loads Parameter.ipdfserver at
        // application startup through GetIPDFServerFromDB() -> GetIPDFServer().
        // This page is Loader-created later, so replay that existing cached DB
        // value first. updateIPServerDF() is an existing iScreenDF function and
        // emits the existing updateServeripDfserver signal; no new query/API or
        // persistence path is introduced.
        if (typeof km.updateIPServerDF === "function")
            km.updateIPServerDF()

        // Keep the legacy Network2 refresh for LAN/global-offset state. Because
        // updateServeripDfserver is replayed first, appliedDfServerIp is already
        // set and Network2.krakenserver cannot overwrite Parameter.ipdfserver.
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
        statusText = "Applying DF Server IP and reconnecting..."

        // NET-ENDPOINTS1.2: preserve the proven legacy TopNetworkDrawer Apply
        // bridge before invoking the real iScreenDF mutation owner. The current
        // Mainwindows hook is compatibility/logging-only, while connectToDFserver()
        // persists Parameter.ipdfserver and reconnects DF TCP/GPSD.
        if (typeof mainWindows !== "undefined" && mainWindows &&
                typeof mainWindows.setNetworkFormDisplay === "function")
            mainWindows.setNetworkFormDisplay(ip)

        km.connectToDFserver(ip)

        // Legacy Apply has no completion/result signal. Keep the field aligned
        // with the operator-requested value; the existing startup/backend
        // updateServeripDfserver signal can still overwrite it when emitted.
        root.appliedDfServerIp = ip
        root.dfServerIp = ip
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

        // Reuse the original DF-server connection backend. Passing the last
        // loaded/applied value avoids reconnecting an unsaved TextField draft.
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

            // Network2 is the legacy DB fallback used by TopNetworkDrawer.
            // Do not let a later Network2 refresh overwrite a DF endpoint that
            // has already been supplied by updateServeripDfserver() or Apply.
            if (serverIp.length > 0 && root.appliedDfServerIp.length === 0) {
                root.appliedDfServerIp = serverIp
                root.dfServerIp = serverIp
                if (!dfServerField.activeFocus)
                    dfServerField.text = serverIp
            }
        }

        function onUpdateServeripDfserver(ip) {
            var normalized = String(ip || "").trim()
            root.appliedDfServerIp = normalized
            root.dfServerIp = normalized
            if (!dfServerField.activeFocus)
                dfServerField.text = normalized
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
