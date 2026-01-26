import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import QtPositioning 5.5
import QtLocation 5.6
import QtQuick.Controls.Material 2.15
import QtGraphicalEffects 1.12
import QtQuick.VirtualKeyboard 2.4

import "iScreenDFqml/pages"
import "iScreenDFqml/popuppanels"
import "iScreenDFqml/sidepanels"
import "iRecordManage"
import "./"

Item {
    id: mainPage
    width: 1920
    height: 1080

    property bool savedDaqVisible: true
    property bool savelockFadeButton: true
    Material.theme: Material.Dark
    Material.accent: Material.Teal
    property bool daqLocked: false
    property string signalStrength: "144"
    property string receiverGain: "0.9 dB"
    property bool keyfreqEdit: false
    signal receiverParamsUpdated(string signalStrength, string receiverGain)
    property string currentPageSource: "qrc:/HomeDisplay.qml"

    property var originalVfoConfig: ({
        spectrum: "Single Ch",
        mode: "Standard",
        activeVfos: 1,
        outputVfo: 0,
        dspDecimation: 1,
        optimizeShortBursts: false
    })

    property var pageSelectorProxy: QtObject {
        property string currentText: ""
    }

    Connections {
        target: Krakenmapval

        function onOpenPopupSettingRequested(msg) {
            console.log("onAddGroupRequested :" + msg)
            popupSetting.openWithMessage(msg)
        }

        function onUpdateParameterModePopup(mode) {
            console.log("[mainPage] updateParameterMode =", mode)
            remoteModePopup.remoteStatus = mode

            if (mode === "LOCAL") {
                remoteModePopup.open()
            } else {
                if (remoteModePopup.visible)
                    remoteModePopup.close()
            }
        }

        function onRequestRemotePopup() {
            console.log("RemoteStatus is LOCAL → showing ModePopup")
            remoteModePopup.open()
        }
    }

    StackView {
        id: loader
        anchors.fill: parent
        initialItem: "qrc:/HomeDisplay.qml"
        z: 1
    }

    QMLMap { id: myMap }

    // ================================ NAV BAR ================================
    Rectangle {
        id: navBar
        width: parent.width
        height: 60
        color: "#111212"
        z: 1
        anchors.top: parent.top

        // ----- GPS cache -----
        property double gpsLat: 0
        property double gpsLong: 0
        property double gpsAlt: 0
        property string utmText: ""
        property string mgrsText: ""

        // ✅ เวลา/วันที่ที่เอาไป bind กับ Label
        property string gpsTimeText: ""
        property string gpsDateText: ""
        property string uptimeText: ""

        // =================== LOCAL CLOCK (Date.now) ===================
        function pad2(v) { v = Math.floor(v); return (v < 10 ? "0" + v : "" + v) }

        function updateLocalClock() {
            var d = new Date(Date.now())
            gpsTimeText = pad2(d.getHours()) + ":" + pad2(d.getMinutes()) + ":" + pad2(d.getSeconds())
            gpsDateText = d.getFullYear() + "-" + pad2(d.getMonth() + 1) + "-" + pad2(d.getDate())
        }

        Timer {
            id: localClockTimer
            interval: 1000
            repeat: true
            running: true
            triggeredOnStart: true
            onTriggered: navBar.updateLocalClock()
        }

        Component.onCompleted: updateLocalClock()
        // ===============================================================

        function formatMGRS(s) {
            if (!s) return "-"
            var t = String(s).trim().replace(/\s+/g, "")
            t = t.toUpperCase().replace(/[^0-9A-Z]/g, "")

            if (t.length < 5) return t

            var zone  = t.slice(0, 2)
            var band  = t.slice(2, 3)
            var grid  = t.slice(3, 5)
            var rest  = t.slice(5)

            if (!/^\d*$/.test(rest) || (rest.length % 2) !== 0 || rest.length === 0)
                return zone + band + " " + grid + (rest.length ? (" " + rest) : "")

            var half = rest.length / 2
            var e = rest.slice(0, half)
            var n = rest.slice(half)

            return zone + band + " " + grid + " " + e + " " + n
        }

        function formatUTM(s) {
            if (!s) return "-"
            return String(s).trim().replace(/\s+/g, " ")
        }

        RowLayout {
            id: navRow
            anchors.fill: parent
            spacing: 20
            anchors.leftMargin: 20
            anchors.rightMargin: 20

            RoundButton {
                id: settingsButton
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                Layout.alignment: Qt.AlignVCenter
                radius: 24
                padding: 6

                background: Rectangle {
                    color: "#111212"
                    border.color: "#373640"
                    border.width: settingsButton.hovered ? 2 : 1
                    radius: 24
                    anchors.fill: parent
                    Behavior on border.width { NumberAnimation { duration: 120 } }
                }

                Item {
                    width: 32
                    height: 32
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
                    topDrawer.close()
                    settingsDrawer.open()
                }
            }

            Item { Layout.fillWidth: true }

            ColumnLayout {
                id: gpsColumn
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                spacing: 2

                Label {
                    id: locationLabel
                    text:
                        "Latitude "  + Number(navBar.gpsLat).toFixed(6)  + "°N " +
                        "Longitude " + Number(navBar.gpsLong).toFixed(6) + "°E " +
                        "Altitude "  + Number(navBar.gpsAlt).toFixed(2)  + "m"
                    color: "#169976"
                    font.pixelSize: 17
                    horizontalAlignment: Text.AlignRight
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignRight
                    spacing: 6

                    Label {
                        id: gps_mgrs
                        text: "MGRS: " + navBar.formatMGRS(navBar.mgrsText) +
                              "   UTM: " + navBar.formatUTM(navBar.utmText)
                        color: "#169976"
                        font.pixelSize: 17
                        horizontalAlignment: Text.AlignRight
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Label {
                        id: timeLabel
                        text: navBar.gpsTimeText && navBar.gpsTimeText.length ? navBar.gpsTimeText : "--:--:--"
                        color: "#7AE2CF"
                        font.pixelSize: 17
                    }

                    Label {
                        id: dateLabel
                        text: navBar.gpsDateText && navBar.gpsDateText.length ? navBar.gpsDateText : "---- -- --"
                        color: "#7AE2CF"
                        font.pixelSize: 17
                    }
                }
            }
        }

        // ================= Center Grab Handle (CLICK + DRAG) =================
        Item {
            id: centerGrabHandle
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            width: 92
            height: 34
            z: 10

            property color accent: "#00FFF0"
            property color idleBar: "#7AE2CF"
            property color textCol: "#9aa6b2"
            property real  thresholdPx: 18

            property bool hovering: hoverArea.containsMouse
            property bool pressing: pressArea.pressed
            property bool dragging: dragHandler.active

            property real startY: 0
            property bool actionDone: false

            Rectangle {
                id: glowPill
                anchors.centerIn: parent
                width: 84
                height: 22
                radius: height / 2
                color: "#000000"
                opacity: centerGrabHandle.dragging ? 0.20
                      : centerGrabHandle.pressing ? 0.18
                      : centerGrabHandle.hovering ? 0.14
                      : 0.08
                border.width: 1
                border.color: centerGrabHandle.dragging ? centerGrabHandle.accent
                            : centerGrabHandle.pressing ? centerGrabHandle.accent
                            : centerGrabHandle.hovering ? "#2A3A44"
                            : "transparent"
                y: centerGrabHandle.dragging ? -2
                 : centerGrabHandle.pressing ? -2
                 : centerGrabHandle.hovering ? -1
                 : 0
                Behavior on opacity { NumberAnimation { duration: 160 } }
                Behavior on y       { NumberAnimation { duration: 160; easing.type: Easing.InOutQuad } }
                Behavior on border.color { ColorAnimation { duration: 160 } }
            }

            Rectangle {
                id: handleBar
                anchors.centerIn: parent
                width: centerGrabHandle.dragging ? 56
                     : centerGrabHandle.pressing ? 54
                     : centerGrabHandle.hovering ? 52
                     : 48
                height: 6
                radius: 3
                color: centerGrabHandle.dragging ? centerGrabHandle.accent : centerGrabHandle.idleBar
                opacity: 0.92
                y: centerGrabHandle.dragging ? -3
                 : centerGrabHandle.pressing ? -3
                 : centerGrabHandle.hovering ? -1
                 : 0
                scale: centerGrabHandle.pressing ? 0.96 : 1.0
                Behavior on scale { NumberAnimation { duration: 110; easing.type: Easing.OutQuad } }
                Behavior on width { NumberAnimation { duration: 140; easing.type: Easing.InOutQuad } }
                Behavior on color { ColorAnimation { duration: 140 } }
                Behavior on y     { NumberAnimation { duration: 140; easing.type: Easing.InOutQuad } }

                Rectangle {
                    id: ripple
                    anchors.centerIn: parent
                    width: 6
                    height: 6
                    radius: width / 2
                    color: centerGrabHandle.accent
                    opacity: 0.0
                    scale: 0.2
                }
                ParallelAnimation {
                    id: rippleAnim
                    running: false
                    NumberAnimation { target: ripple; property: "opacity"; from: 0.35; to: 0.0; duration: 240; easing.type: Easing.OutQuad }
                    NumberAnimation { target: ripple; property: "scale";   from: 0.2;  to: 3.0; duration: 240; easing.type: Easing.OutQuad }
                }
            }

            ColorOverlay {
                id: glowOverlay
                anchors.fill: handleBar
                source: handleBar
                color: centerGrabHandle.accent
                opacity: centerGrabHandle.dragging ? 0.55
                      : centerGrabHandle.pressing ? 0.45
                      : centerGrabHandle.hovering ? 0.32
                      : 0.18
                Behavior on opacity { NumberAnimation { duration: 160 } }
            }

            SequentialAnimation {
                id: pulseAnim
                running: true
                loops: Animation.Infinite
                NumberAnimation { target: glowOverlay; property: "opacity"; from: 0.16; to: 0.26; duration: 900; easing.type: Easing.InOutQuad }
                NumberAnimation { target: glowOverlay; property: "opacity"; from: 0.26; to: 0.16; duration: 900; easing.type: Easing.InOutQuad }
            }
            function updatePulse() { pulseAnim.running = !(hovering || pressing || dragging) }
            onHoveringChanged: updatePulse()
            onPressingChanged: updatePulse()
            onDraggingChanged: updatePulse()

            Text {
                id: hintText
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.bottom
                anchors.topMargin: -2
                text: centerGrabHandle.dragging ? "Release"
                     : centerGrabHandle.hovering ? "Click / Drag"
                     : "Drag"
                color: centerGrabHandle.textCol
                font.pixelSize: 11
                opacity: centerGrabHandle.hovering ? 0.60 : 0.0
                Behavior on opacity { NumberAnimation { duration: 160 } }
            }

            MouseArea {
                id: hoverArea
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
                cursorShape: Qt.SizeVerCursor
            }

            MouseArea {
                id: pressArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    rippleAnim.stop()
                    ripple.opacity = 0.35
                    ripple.scale = 0.2
                    rippleAnim.start()

                    if (!topDrawer) return
                    if (topDrawer.visible) topDrawer.close()
                    else topDrawer.open()
                }
            }

            DragHandler {
                id: dragHandler
                target: null
                grabPermissions: PointerHandler.CanTakeOverFromAnything
                yAxis.enabled: true
                xAxis.enabled: false

                onActiveChanged: {
                    if (active) {
                        centerGrabHandle.startY = centroid.position.y
                        centerGrabHandle.actionDone = false
                    } else {
                        centerGrabHandle.actionDone = false
                    }
                }

                onCentroidChanged: {
                    if (!active || centerGrabHandle.actionDone) return
                    var dy = centroid.position.y - centerGrabHandle.startY

                    if (dy > centerGrabHandle.thresholdPx) {
                        if (!topDrawer.visible) topDrawer.open()
                        centerGrabHandle.actionDone = true
                    } else if (dy < -centerGrabHandle.thresholdPx) {
                        if (topDrawer.visible) topDrawer.close()
                        centerGrabHandle.actionDone = true
                    }
                }
            }
        }
    }

    // ==================== OPTIONAL: ถ้า C++ ส่งมา ก็รับทับได้ ====================
    Connections {
        id: timeGpsConn
        target: Krakenmapval
        ignoreUnknownSignals: true

        function onUpdateLocalTime(currentTime, currentDate, uptime) {
            // ถ้าต้องการให้ C++ เป็นตัวจริง ให้ทับของ Date.now ได้เลย
            navBar.gpsTimeText = currentTime
            navBar.gpsDateText = currentDate
            navBar.uptimeText  = uptime
        }

        function onUpdateLocationLatLongFromGPS(latStr, lonStr, altStr, utmText, mgrsText) {
            navBar.gpsLat   = parseFloat(latStr)
            navBar.gpsLong  = parseFloat(lonStr)
            navBar.gpsAlt   = parseFloat(altStr)
            navBar.utmText  = utmText
            navBar.mgrsText = mgrsText
        }
    }

    // ================================ FLOATING SCREENSHOT BUTTON ================================
    Item {
        id: floatingLayer
        width: 48
        height: 48
        z: 3
        visible: true
        focus: false
        anchors.top: navBar.top
        anchors.left: parent.left
        anchors.topMargin: 6
        anchors.leftMargin: 90

        Rectangle {
            id: floatingButton
            width: 48
            height: 48
            radius: 24
            color: "transparent"
            border.color: floatingMouseArea.containsMouse ? "#00FFF0" : "transparent"
            border.width: floatingMouseArea.containsMouse ? 2 : 1
            anchors.fill: parent

            Item {
                width: 48
                height: 48
                Image {
                    id: homeIcon
                    anchors.fill: parent
                    source: "qrc:/iScreenDFqml/images/screenshot.png"
                    fillMode: Image.PreserveAspectFit
                    visible: false
                }
                ColorOverlay {
                    anchors.fill: homeIcon
                    source: homeIcon
                    color: "#696969"
                }
            }

            MouseArea {
                id: floatingMouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: getScreenshotTimer.start()
            }
        }
    }

    Timer {
        id: getScreenshotTimer
        interval: 2000
        running: false
        repeat: false
        onTriggered: window.getScreenshot()
    }

    // ================================ DRAWERS / POPUPS ================================
    SideSettingsDrawer {
        id: settingsDrawer
        krakenmapval: Krakenmapval
        width: 500
        height: parent.height
        z: 1

        onNavigate: function(title, source, index) {
            if (source !== "qrc:/HomeDisplay.qml") {
                if (loader.depth > 1) loader.pop()
                loader.push(source)
            } else {
                while (loader.depth > 1) loader.pop()
            }
            currentPageSource = source
            pageSelectorProxy.currentText = title
        }
    }

    PopupSettingDrawer {
        id: popupSetting
        width: 1100
        height: 600
        anchors.top: loader.bottom
        anchors.bottom: loader.top
        anchors.leftMargin: -1417
        anchors.rightMargin: -1603
        anchors.topMargin: -990
        anchors.bottomMargin: -690
        z: 10000
        visible: false
        anchors.left: loader.right
        anchors.right: loader.left
        krakenmapval: Krakenmapval
    }

    TopNetworkDrawer {
        id: topDrawer
        krakenmapval: Krakenmapval
        keyfreqEdit: mainPage.keyfreqEdit
    }

    ModePopup { id: remoteModePopup }
}
