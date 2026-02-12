// /pages/SideSettingsDrawer.qml  (FULL FILE)
// 1920x1080 overlay + drawer width เดิม 500
// click นอก drawer => close (ชัวร์สุด)
//
// ✅ ADD: ONLINE/OFFLINE button next to LOCAL/REMOTES
// - toggles map style mode (online/offline)
// - broadcasts via Krakenmapval (recommended, works across pages)

import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.3
import QtPositioning 5.5
import QtLocation 5.6
import QtQuick.Controls.Material 2.12
import QtGraphicalEffects 1.0
import QtQuick.VirtualKeyboard 2.4
import Qt.labs.settings 1.1

import "../i18n" as I18n
import "./"

Item {
    z: 100

    id: settingsPanel
    anchors.fill: parent
    width: parent ? parent.width : 1920
    height: parent ? parent.height : 1080

    // ===== Public API =====
    property var krakenmapval: null
    signal navigate(string title, string source, int index)

    property string currentLang: (krakenmapval && krakenmapval.language)
                                 ? krakenmapval.language : "en"

    // ===== Internal for side content =====
    property string sidePanelKey: "localdevice"
    function _sourceForKey(k) {
        switch (k) {
        case "group":       return "qrc:/iScreenDFqml/sidepanels/SideGroup.qml"
        case "remote":      return "qrc:/iScreenDFqml/sidepanels/SideRemote.qml"
        case "localdevice": return "qrc:/iScreenDFqml/sidepanels/SideLocal.qml"
        default:            return "qrc:/iScreenDFqml/sidepanels/SideLocal.qml"
        }
    }
    function _syncSidePanel() { sideLoader.source = _sourceForKey(sidePanelKey) }

    Settings {
        id: uiSettings
        category: "SideSettingsDrawer"
        property bool savedUseOfflineMap: false   // false=ONLINE, true=OFFLINE
    }
    property bool useOffline: uiSettings.savedUseOfflineMap
    // ===== States (ควบคุมเฉพาะ drawer) =====
    state: "closed"
    states: [
        State { name: "open";   PropertyChanges { target: drawer; x: 0 } },
        State { name: "closed"; PropertyChanges { target: drawer; x: -drawer.width } }
    ]
    transitions: Transition {
        NumberAnimation { target: drawer; properties: "x"; duration: 300; easing.type: Easing.InOutQuad }
    }

    // ===== Dim overlay =====
    Rectangle {
        id: overlay
        anchors.fill: parent
        z: 0
        color: "transparent"
        opacity: 1.0
        visible: settingsPanel.state === "open"
    }

    // ===== Drawer panel =====
    Rectangle {
        id: drawer
        width: 500
        height: parent.height
        color: "#111212"
        z: 10
        x: -width

        Item {
            id: _item
            anchors.fill: parent
            anchors.margins: 15

            Connections {
                target: Krakenmapval
                function onUpdateReceiverParametersFreqandbw(freq, bw, link) {
                    if (!link) {
                        console.log("[QML] link=false, not sending frequency")
                        return
                    }

                    if (typeof mainWindows !== "undefined"
                        && mainWindows
                        && typeof mainWindows.sendmessage === "function") {

                        var msgObj = {
                            type: "setfrequency",
                            params: {
                                frequency: freq,
                                bandwidth: bw,
                                key: "memagic"
                            }
                        }

                        var msg = JSON.stringify(msgObj)
                        mainWindows.sendmessage(msg)
                        console.log("[sendmessage]", msg)
                    } else {
                        console.warn("mainWindows.sendmessage not available")
                    }

                    if (typeof spectrumCanvas !== "undefined"
                        && spectrumCanvas
                        && spectrumCanvas.clearPeakTimer) {
                        spectrumCanvas.clearPeakTimer.start()
                    }
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                anchors.leftMargin: 5
                anchors.topMargin: -8
                anchors.bottomMargin: 0
                spacing: 10

                // ====== แถวปุ่มเมนู + สลับโหมด ======
                RowLayout {
                    id: headerRow
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                    spacing: 12

                    RoundButton {
                        id: settingsButton
                        Layout.preferredWidth: 48
                        Layout.preferredHeight: 48
                        radius: 24
                        padding: 6

                        background: Rectangle {
                            color: settingsButton.pressed ? "#111212" :
                                   settingsButton.hovered ? "#111212" : "#111212"
                            border.color: "#373640"
                            border.width: settingsButton.hovered ? 2 : 1
                            radius: 24
                            anchors.fill: parent
                            Behavior on color { ColorAnimation { duration: 200 } }
                        }

                        Item {
                            width: 32; height: 32
                            anchors.centerIn: parent
                            Column {
                                spacing: 6
                                anchors.centerIn: parent
                                Rectangle { width: 20; height: 3; radius: 1.5; color: "#7AE2CF" }
                                Rectangle { width: 20; height: 3; radius: 1.5; color: "#7AE2CF" }
                                Rectangle { width: 20; height: 3; radius: 1.5; color: "#7AE2CF" }
                            }
                        }

                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Menu")

                        onClicked: {
                            settingsPanel.close()
                            if (typeof popupSetting !== "undefined" && popupSetting)
                                popupSetting.close()
                        }
                    }

                    // ===================== LOCAL/REMOTES switch =====================
                    Rectangle {
                        id: connSwitch
                        Layout.preferredHeight: 40
                        Layout.preferredWidth: 150
                        Layout.alignment: Qt.AlignVCenter
                        radius: height / 2
                        color: hovered ? "#25303b" : "transparent"
                        border.width: 2
                        Layout.leftMargin: 70
                        border.color: "#25303b"

                        property bool isLocal: true
                        property bool hovered: false
                        readonly property string labelText: isLocal ? "LOCAL" : "REMOTES"

                        Row {
                            anchors.centerIn: parent
                            spacing: 10
                            Text {
                                text: connSwitch.labelText
                                color: "#ffffff"
                                font.pixelSize: 16
                                font.bold: true
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: connSwitch.hovered = true
                            onExited:  connSwitch.hovered = false
                            onClicked: {
                                connSwitch.isLocal = !connSwitch.isLocal
                                Krakenmapval.setMode(connSwitch.isLocal ? "LOCAL" : "REMOTES")
                            }
                        }

                        Connections {
                            target: Krakenmapval
                            function onUpdateParameterMode(mode) {
                                connSwitch.isLocal = (mode === "LOCAL")
                            }
                        }
                    }

                    // ===================== MAP ONLINE / OFFLINE button (CLEAR UX) =====================
                    Rectangle {
                        id: netStyleSwitch
                        Layout.preferredHeight: 40
                        Layout.preferredWidth: 160
                        Layout.alignment: Qt.AlignVCenter
                        radius: height / 2
                        border.width: 2

                        // false = MAP ONLINE, true = MAP OFFLINE
                        property bool useOffline: true
                        property bool hovered: true

                        // ✅ Colors
                        readonly property color cOnline:  "#163A35"   // dark green
                        readonly property color cOnline2: "#7AE2CF"   // accent
                        readonly property color cOffline: "#3A2A16"   // dark amber
                        readonly property color cOffline2:"#F3B25E"   // accent amber

                        // ✅ Label
                        readonly property string labelText: useOffline ? "MAP OFFLINE" : "MAP ONLINE"
                        readonly property string subText:   useOffline ? "tiles/cache mode" : "internet mode"

                        // ✅ Visual state
                        border.color: useOffline ? cOffline2 : cOnline2
                        color: hovered
                               ? (useOffline ? Qt.darker(cOffline, 1.15) : Qt.darker(cOnline, 1.15))
                               : (useOffline ? cOffline : cOnline)

                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 10

                            // ✅ Map icon
                            Image {
                                Layout.preferredWidth: 22
                                Layout.preferredHeight: 22
                                source: "qrc:/iScreenDFqml/images/earth-asia.png"   // ใช้อันที่มีอยู่แล้ว
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true
                                opacity: 0.95
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0

                                Text {
                                    text: netStyleSwitch.labelText
                                    color: "#FFFFFF"
                                    font.pixelSize: 14
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: netStyleSwitch.subText
                                    color: useOffline ? "#E9D8B5" : "#CFEFEA"
                                    font.pixelSize: 10
                                    opacity: 0.95
                                    elide: Text.ElideRight
                                }
                            }

                            // ✅ Status icon (online/offline)
                            // Item {
                            //     Layout.preferredWidth: 22
                            //     Layout.preferredHeight: 22

                            //     // dot background
                            //     Rectangle {
                            //         anchors.centerIn: parent
                            //         width: 18; height: 18
                            //         radius: 9
                            //         color: "transparent"
                            //         border.width: 2
                            //         border.color: useOffline ? netStyleSwitch.cOffline2 : netStyleSwitch.cOnline2
                            //     }

                            //     // dot fill
                            //     Rectangle {
                            //         anchors.centerIn: parent
                            //         width: 10; height: 10
                            //         radius: 5
                            //         color: useOffline ? netStyleSwitch.cOffline2 : netStyleSwitch.cOnline2
                            //     }
                            // }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: netStyleSwitch.hovered = true
                            onExited:  netStyleSwitch.hovered = false

                            onClicked: {
                                netStyleSwitch.useOffline = !netStyleSwitch.useOffline

                                // ✅ จำค่า
                                uiSettings.savedUseOfflineMap = netStyleSwitch.useOffline

                                // ✅ broadcast to MapViewer page
                                if (Krakenmapval && typeof Krakenmapval.setUseOfflineMapStyle === "function") {
                                    Krakenmapval.setUseOfflineMapStyle(netStyleSwitch.useOffline)
                                } else {
                                    console.warn("Krakenmapval.setUseOfflineMapStyle(bool) not available")
                                }
                            }
                        }

                        ToolTip.visible: hovered
                        ToolTip.text: useOffline
                                      ? "Map OFFLINE: use cached/offline tiles (no internet)"
                                      : "Map ONLINE: use internet map tiles"

                        // ✅ Optional: sync from backend when app starts
                        Connections {
                            target: Krakenmapval
                            function onUseOfflineMapStyleChanged(useOffline) {
                                var v = !!useOffline
                                if (netStyleSwitch.useOffline === v) return

                                netStyleSwitch.useOffline = v

                                // ✅ จำค่า (กันเปิดใหม่แล้วเพี้ยน)
                                uiSettings.savedUseOfflineMap = v
                            }
                        }
                    }
                }

                Label {
                    text: "Select Mode"
                    font.pixelSize: 20
                    font.bold: true
                    color: "#ffffff"
                }

                Row {
                    id: toolbar
                    spacing: 16
                    anchors.margins: 10
                    anchors.horizontalCenter: parent.horizontalCenter
                    property int hoveredIndex: -1
                    property var pages: [
                        { title: "RADIO",              icon: "qrc:/images/radioIcon.png",             source: "qrc:/HomeDisplay.qml" },
                        { title: "MAP\nVISUALIZATION", icon: "qrc:/iScreenDFqml/images/earth-asia.png", source: "qrc:/iScreenDFqml/pages/QMLMap.qml" },
                        { title: "DOA\nVIEWER",        icon: "qrc:/iScreenDFqml/images/dart-board.png", source: "qrc:/DoaViewer/ViewerPage.qml" },
                        { title: "RECORDER",           icon: "qrc:/iRecordManage/images/IconRec.png",  source: "qrc:/iRecordManage/TapBarRecordFiles.qml" },
                        { title: "DIAG\nNOSTIC",       icon: "qrc:/images/Diagnostic.png",            source: "qrc:/iRecordManage/MonitorDisplay.qml" }
                    ]
                    property int currentIndex: 0

                    Repeater {
                        model: toolbar.pages
                        ToolButton {
                            id: btn
                            width: 80; height: 80
                            checkable: true
                            checked: index === toolbar.currentIndex

                            hoverEnabled: true

                            contentItem: Column {
                                spacing: 4
                                anchors.centerIn: parent
                                Image {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    source: modelData.icon
                                    width: 40; height: 40
                                    fillMode: Image.PreserveAspectFit
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: modelData.title
                                    color: (btn.checked || index === toolbar.hoveredIndex)
                                           ? "#FFFFFF" : "#AAAAAA"
                                    font.pixelSize: 10
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }

                            background: Rectangle {
                                radius: 8
                                color: btn.checked
                                       ? "#6c9386"
                                       : (index === toolbar.hoveredIndex ? "#6c9386" : "transparent")
                                border.color: (index === toolbar.hoveredIndex) ? "#6c9386" : "transparent"
                                border.width: 1
                            }

                            onHoveredChanged: {
                                if (hovered) {
                                    toolbar.hoveredIndex = index
                                } else {
                                    if (toolbar.hoveredIndex === index
                                        && toolbar.currentIndex !== index) {
                                        toolbar.hoveredIndex = -1
                                    }
                                }
                            }

                            onClicked: {
                                if (index < 0 || index >= toolbar.pages.length) return

                                var oldSource = toolbar.pages[toolbar.currentIndex].source
                                var newSource = modelData.source

                                if (oldSource === "qrc:/DoaViewer/ViewerPage.qml"
                                    && newSource !== oldSource) {

                                    if (typeof doaClient !== "undefined"
                                        && doaClient
                                        && doaClient.connected) {

                                        doaClient.disconnectFromServer()
                                    }
                                }

                                toolbar.currentIndex = index
                                toolbar.hoveredIndex = index

                                settingsPanel.navigate(modelData.title, modelData.source, index)

                                if (newSource === "qrc:/DoaViewer/ViewerPage.qml") {
                                    if (typeof doaClient !== "undefined"
                                        && doaClient
                                        && !doaClient.connected) {

                                        doaClient.connectToServer()
                                    }
                                }

                                var MAP_SOURCE = "qrc:/iScreenDFqml/pages/QMLMap.qml"
                                var enteringMap = (newSource === MAP_SOURCE)

                                var km = (typeof krakenmapval !== "undefined" && krakenmapval) ? krakenmapval
                                        : ((typeof Krakenmapval !== "undefined" && Krakenmapval) ? Krakenmapval : null)

                                if (km) {
                                    if (enteringMap) {
                                        if (typeof Krakenmapval.sendSetSpectrumEnable === "function")
                                            Krakenmapval.sendSetSpectrumEnable(false)

                                        if (typeof Krakenmapval.requestRfFrequency === "function")
                                            Krakenmapval.requestRfFrequency()
                                    }
                                }
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    height: 44

                    PillSegmentBar {
                        id: bar
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 320
                        model: [
                            { iconSource: "qrc:/iScreenDFqml/images/remote-access.png", tooltip: "Local Device",  sidePanel: "localdevice" },
                            { iconSource: "qrc:/iScreenDFqml/images/object-group.png",  tooltip: "Group Device",  sidePanel: "group" },
                            { iconSource: "qrc:/iScreenDFqml/images/remotedevice.png",  tooltip: "Remote Device", sidePanel: "remote" }
                        ]
                        onTriggered: function(i) {
                            const item = bar.model[i]
                            if (item && item.sidePanel) {
                                bar.currentIndex = i
                                settingsPanel.sidePanelKey = item.sidePanel
                                settingsPanel._syncSidePanel()
                            }
                        }
                    }
                }

                Label {
                    text: "RF Receiver Configuration"
                    font.pixelSize: 20
                    font.bold: true
                    color: "#ffffff"
                }

                GridLayout {
                    id: vfoConfigLayout
                    columns: 2
                    rowSpacing: 30
                    columnSpacing: 30
                    Layout.rightMargin: 0
                    property bool vfoFormInitialized: false

                    Connections {
                        target: settingsPanel.krakenmapval
                        function onRfsocParameterUpdated(frequencyHz, doaBwHz) {
                            offcentersetInput.text = (frequencyHz / 1e6).toFixed(4)

                            var bwHz = Number(doaBwHz)
                            if (isFinite(bwHz) && bwHz > 0) {
                                bwInput.text = (bwHz / 1000.0).toFixed(3)
                            } else {
                                bwInput.text = ""
                            }
                        }
                    }

                    Label { text: "Frequency:"; font.pixelSize: 16; color: "#ffffff" }

                    RowLayout {
                        Layout.preferredWidth: 260
                        spacing: 8

                        TextField {
                            id: offcentersetInput
                            Layout.preferredWidth: 170
                            Layout.preferredHeight: 32
                            font.pixelSize: 16
                            color: "#7AE2CF"
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignLeft
                            leftPadding: 10
                            rightPadding: 10
                            topPadding: 4
                            bottomPadding: 4
                            placeholderTextColor: "#666"
                            placeholderText: "-500 .. 500"
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            validator: DoubleValidator { bottom: -10000000.0; top: 10000000.0 }

                            background: Rectangle {
                                color: "#111A1E"
                                radius: 10
                                border.color: offcentersetInput.activeFocus ? "#7AE2CF" : "#1B8F77"
                                border.width: 1
                            }

                            Keys.onReturnPressed: focus = false
                            Keys.onEnterPressed:  focus = false
                            onFocusChanged: if (focus) selectAll()
                        }

                        ToolButton {
                            id: connectBtn
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            property bool connected: false

                            background: Rectangle {
                                radius: 8
                                color: connectBtn.hovered ? "#163A35" : "#111A1E"
                                border.color: "transparent"
                                border.width: 1
                            }

                            indicator: Image {
                                anchors.centerIn: parent
                                width: 20
                                height: 20
                                source: connectBtn.connected
                                        ? "qrc:/iScreenDFqml/images/chain_link.png"
                                        : "qrc:/iScreenDFqml/images/chain_unlink.png"
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true

                                onStatusChanged: {
                                    if (status === Image.Error)
                                        console.log("[ICON] LOAD ERROR:", source)
                                }
                            }

                            Connections {
                                target: settingsPanel.krakenmapval
                                function onUpdatelinkStatus(link) {
                                    console.log("[QML] updatelinkStatus =", link, typeof link)
                                    connectBtn.connected = link
                                }
                            }

                            onClicked: {
                                connectBtn.connected = !connectBtn.connected
                                console.log("Connect:", connectBtn.connected)
                                if (Krakenmapval && typeof Krakenmapval.setLinkStatus === "function") {
                                    Krakenmapval.setLinkStatus(connectBtn.connected)
                                } else {
                                    console.warn("setLinkStatusFromQml() not available on krakenmapval")
                                }
                            }

                            ToolTip.visible: hovered
                            ToolTip.text: connectBtn.connected ? "Connected" : "Disconnected"
                        }

                        Text {
                            text: "MHz"
                            color: "#9CA3AF"
                            font.pixelSize: 14
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: 36
                        }
                    }

                    Label { text: "Bandwidth:"; font.pixelSize: 16; color: "#ffffff" }

                    RowLayout {
                        Layout.preferredWidth: 260
                        spacing: 8

                        TextField {
                            id: bwInput
                            Layout.preferredWidth: 200
                            Layout.preferredHeight: 32
                            font.pixelSize: 16
                            color: "#7AE2CF"
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignLeft
                            leftPadding: 10
                            rightPadding: 10
                            topPadding: 4
                            bottomPadding: 4
                            placeholderTextColor: "#666"
                            placeholderText: "0.05 .. 10000"
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            validator: DoubleValidator { bottom: 0.05; top: 10000.0; decimals: 3 }

                            background: Rectangle {
                                color: "#111A1E"
                                radius: 10
                                border.color: bwInput.activeFocus ? "#7AE2CF" : "#1B8F77"
                                border.width: 1
                            }

                            Keys.onReturnPressed: focus = false
                            Keys.onEnterPressed:  focus = false
                            onFocusChanged: if (focus) selectAll()
                        }

                        Text { text: "kHz"; color: "#9CA3AF"; font.pixelSize: 14; Layout.preferredWidth: 40 }
                    }

                    Button {
                        id: updateButton
                        text: "Update Receiver Parameters"
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        height: 36
                        font.pixelSize: 16
                        font.bold: true

                        background: Rectangle {
                            color: updateButton.pressed ? Qt.darker("#169976", 1.3) : "#169976"
                            radius: 6
                        }

                        contentItem: Text {
                            text: updateButton.text
                            anchors.fill: parent
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            color: "#212121"
                            font.bold: true
                            font.pixelSize: updateButton.font.pixelSize
                        }

                        onClicked: {
                            if (!settingsPanel.krakenmapval) return

                            var mhz = Number(offcentersetInput.text)
                            var bwKhz = Number(bwInput.text)
                            var bwHz  = bwKhz * 1000.0

                            if (isNaN(mhz) || isNaN(bwKhz) || !isFinite(bwHz)) {
                                console.warn("Invalid input")
                                return
                            }

                            var freqHz = Math.round(mhz * 1e6)

                            console.log("updateReceiverParameters freqHz=" + freqHz + " bwHz=" + bwHz +
                                        " connected=" + connectBtn.connected)

                            Krakenmapval.updateReceiverParametersFreqandbw(freqHz, bwHz)

                            if (connectBtn.connected) {
                                if (typeof mainWindows !== "undefined" && mainWindows && typeof mainWindows.sendmessage === "function") {
                                    var msg = '{"type":"setfrequency","params":{"frequency":' + freqHz + ',"key":"memagic"}}'
                                    mainWindows.sendmessage(msg)
                                    console.log("[sendmessage] " + msg)
                                } else {
                                    console.warn("mainWindows.sendmessage not available")
                                }

                                if (typeof spectrumCanvas !== "undefined" && spectrumCanvas && spectrumCanvas.clearPeakTimer) {
                                    spectrumCanvas.clearPeakTimer.start()
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 10
                    color: "#111212"
                    border.color: "#111212"
                    border.width: 1
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 1

                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        Loader {
                            id: sideLoader
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.topMargin: 6
                            anchors.bottom: parent.bottom
                            anchors.top: parent.top

                            onLoaded: {
                                if (item && settingsPanel.krakenmapval && item.hasOwnProperty("krakenmapval"))
                                    item.krakenmapval = settingsPanel.krakenmapval
                            }
                        }
                    }
                }
            }
        }
    }

    // ✅ Click outside drawer to close (ROCK-SOLID)
    MouseArea {
        id: clickOutsideToClose
        anchors.fill: parent
        z: 999999
        enabled: settingsPanel.state === "open"
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        preventStealing: true
        propagateComposedEvents: true

        function isInsideDrawer(x, y) {
            var p = mapToItem(drawer, x, y)
            return (p.x >= 0 && p.x <= drawer.width && p.y >= 0 && p.y <= drawer.height)
        }

        onPressed: function(mouse) {
            if (isInsideDrawer(mouse.x, mouse.y)) {
                mouse.accepted = false
                return
            }
            mouse.accepted = true
            settingsPanel.close()
        }

        onReleased: function(mouse) {
            if (!isInsideDrawer(mouse.x, mouse.y))
                mouse.accepted = true
            else
                mouse.accepted = false
        }

        onClicked: function(mouse) {
            if (!isInsideDrawer(mouse.x, mouse.y)) {
                mouse.accepted = true
                settingsPanel.close()
            } else {
                mouse.accepted = false
            }
        }
    }

    // ===== Functions =====
    function open() {
        settingsPanel.state = "open"
        _syncSidePanel()

        if (Krakenmapval && typeof Krakenmapval.setUseOfflineMapStyle === "function") {
            Krakenmapval.setUseOfflineMapStyle(uiSettings.savedUseOfflineMap)
        }
    }


    function close() { settingsPanel.state = "closed" }

    Component.onCompleted: {
        _syncSidePanel()

        // ✅ ถ้า backend พร้อม ให้ sync ค่า “ที่จำไว้” ไปยังระบบ (ครั้งเดียว)
        if (Krakenmapval && typeof Krakenmapval.setUseOfflineMapStyle === "function") {
            Krakenmapval.setUseOfflineMapStyle(uiSettings.savedUseOfflineMap)
        }
    }
}
