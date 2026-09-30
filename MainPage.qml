import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import QtPositioning 5.5
import QtLocation 5.6
import QtQuick.Controls.Material 2.15
import QtGraphicalEffects 1.12
import QtQuick.VirtualKeyboard 2.4
import Qt.labs.settings 1.1

import "ui"
import "iScreenDFqml/pages"
import "iScreenDFqml/popuppanels"
import "iScreenDFqml/sidepanels"
import "iRecordManage"
import "./"

Item {
    id: mainPage
    width: parent ? parent.width : 1920
    height: parent ? parent.height : 1080
    property bool savedDaqVisible: true
    property bool savelockFadeButton: true
    readonly property bool compactShell: width < hmiTheme.compactWidth
    readonly property bool narrowShell: width < hmiTheme.narrowWidth

    Settings {
        id: shellSettings
        category: "iScanMR10.HmiShell"
        property bool darkTheme: true
    }

    property alias darkTheme: shellSettings.darkTheme
    Theme { id: hmiTheme; darkMode: mainPage.darkTheme }

    Material.theme: darkTheme ? Material.Dark : Material.Light
    // Explicit Material palette keeps Controls2 popups/dialogs readable even
    // when Qt creates their visual content under Overlay rather than the page.
    Material.background: hmiTheme.panel
    Material.foreground: hmiTheme.text
    Material.primary: hmiTheme.accent
    Material.accent: hmiTheme.accent
    property bool daqLocked: false
    property string signalStrength: "144"
    property string receiverGain: "0.9 dB"
    property bool keyfreqEdit: false
    property string operationMode: "LOCAL"
    // R1.7.4C: disable the old Remote Mode text overlay.
    // On touch screens it could remain visible until the user tapped the text.
    readonly property bool remoteModePopupEnabled: false
    signal receiverParamsUpdated(string signalStrength, string receiverGain)
    property string currentPageSource: "qrc:/HomeDisplay.qml"
    // User requested to remove the persistent left navigation rail because
    // SideSettingsDrawer already provides page navigation.
    readonly property bool primaryNavigationEnabled: false
    // UX-NAV1: remove the legacy top Network Settings drawer handle.
    // Network Settings is still opened from the normal menu.
    readonly property bool topNetworkDrawerEnabled: false

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
            mainPage.operationMode = mode
            remoteModePopup.remoteStatus = mode

            // R1.7.4C: keep the internal mode/status update, but do not show
            // the old text overlay/popup when mode changes.
            if (remoteModePopup.visible)
                remoteModePopup.close()

            if (mainPage.remoteModePopupEnabled && mode === "LOCAL")
                remoteModePopup.open()
        }

        function onRequestRemotePopup() {
            console.log("RemoteStatus is LOCAL → ModePopup disabled by R1.7.4C")
            if (remoteModePopup.visible)
                remoteModePopup.close()

            if (mainPage.remoteModePopupEnabled)
                remoteModePopup.open()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: hmiTheme.page
        z: 0
    }

    Component {
        id: homeDisplayComponent
        HomeDisplay {
            darkMode: mainPage.darkTheme
        }
    }

    StackView {
        id: loader
        anchors.fill: parent
        anchors.leftMargin: mainPage.primaryNavigationEnabled ? primaryNavigation.width : 0
        initialItem: homeDisplayComponent
        z: 1
    }

    // Only the currently visible Recorder page owns its TableView/WaveEditor.
    // Forward the optional signal without keeping a second hidden Recorder tree.
    Connections {
        target: loader.currentItem
        ignoreUnknownSignals: true
        function onWavePlayToggleRequested(wantPlay, filesArray, concatMode, playPosMs) {
            if (typeof window !== "undefined" && window.handleRecorderWavePlayToggle)
                window.handleRecorderWavePlayToggle(wantPlay, filesArray, concatMode, playPosMs)
        }
    }

    QMLMap { id: myMap }

    // ================================ NAV BAR ================================
    Rectangle {
        id: navBar
        width: parent.width
        height: hmiTheme.topBarHeight
        color: hmiTheme.topBar
        border.color: hmiTheme.line
        border.width: 1
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

        // Clock ownership:
        // - C++ updateLocalTime is the primary source while it is alive.
        // - Date.now() is a fallback only, so the two sources never fight over
        //   gpsTimeText/gpsDateText and make the date format flip on screen.
        property bool backendClockActive: false
        property double lastBackendClockMs: 0
        readonly property int backendClockTimeoutMs: 3500

        // =================== LOCAL CLOCK (Date.now) ===================
        function pad2(v) {
            v = Math.floor(Number(v))
            return v < 10 ? "0" + v : "" + v
        }

        function monthShortName(monthIndex) {
            var months = ["Jan", "Feb", "Mar", "Apr", "May", "Jun",
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
            return monthIndex >= 0 && monthIndex < months.length ? months[monthIndex] : "---"
        }

        function formatHomeDate(dateValue) {
            if (!dateValue || isNaN(dateValue.getTime()))
                return "-- --- ----"

            return pad2(dateValue.getDate()) + " " +
                   monthShortName(dateValue.getMonth()) + " " +
                   dateValue.getFullYear()
        }

        function normalizeBackendTime(value) {
            var text = value === undefined || value === null ? "" : String(value).trim()
            var match = /^(\d{1,2}):(\d{1,2}):(\d{1,2})$/.exec(text)
            if (!match)
                return text

            return pad2(Number(match[1])) + ":" +
                   pad2(Number(match[2])) + ":" +
                   pad2(Number(match[3]))
        }

        function normalizeBackendDate(value) {
            var text = value === undefined || value === null ? "" : String(value).trim()
            var match

            // yyyy-MM-dd -> dd MMM yyyy
            match = /^(\d{4})-(\d{1,2})-(\d{1,2})$/.exec(text)
            if (match) {
                var isoDate = new Date(Number(match[1]), Number(match[2]) - 1, Number(match[3]))
                return formatHomeDate(isoDate)
            }

            // dd/MM/yyyy or dd-MM-yyyy -> dd MMM yyyy
            match = /^(\d{1,2})[\/-](\d{1,2})[\/-](\d{4})$/.exec(text)
            if (match) {
                var dmyDate = new Date(Number(match[3]), Number(match[2]) - 1, Number(match[1]))
                return formatHomeDate(dmyDate)
            }

            // Already human-readable, for example "01 Jan 1970".
            match = /^(\d{1,2})\s+([A-Za-z]{3})\s+(\d{4})$/.exec(text)
            if (match)
                return pad2(Number(match[1])) + " " + match[2] + " " + match[3]

            // Preserve an unknown backend format rather than guessing an
            // ambiguous month/day ordering.
            return text
        }

        function updateLocalClock() {
            var nowMs = Date.now()

            if (backendClockActive && (nowMs - lastBackendClockMs) <= backendClockTimeoutMs)
                return

            backendClockActive = false
            var d = new Date(nowMs)
            gpsTimeText = pad2(d.getHours()) + ":" + pad2(d.getMinutes()) + ":" + pad2(d.getSeconds())
            gpsDateText = formatHomeDate(d)
        }

        function applyBackendClock(currentTime, currentDate, uptime) {
            backendClockActive = true
            lastBackendClockMs = Date.now()

            var normalizedTime = normalizeBackendTime(currentTime)
            var normalizedDate = normalizeBackendDate(currentDate)

            if (normalizedTime.length > 0)
                gpsTimeText = normalizedTime
            if (normalizedDate.length > 0)
                gpsDateText = normalizedDate
            uptimeText = uptime === undefined || uptime === null ? "" : String(uptime)
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

                property color glyphColor: mainPage.darkTheme ? hmiTheme.accentHover : hmiTheme.accent

                background: Rectangle {
                    color: mainPage.darkTheme
                           ? (settingsButton.hovered ? hmiTheme.card : hmiTheme.cardAlt)
                           : (settingsButton.hovered ? "#F4FBF9" : "#FFFFFF")
                    border.color: mainPage.darkTheme
                                  ? (settingsButton.hovered ? hmiTheme.accentHover : hmiTheme.line)
                                  : (settingsButton.hovered ? hmiTheme.accent : "#A7C3BC")
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
                        Rectangle { width: 20; height: 3; radius: 1.5; color: settingsButton.glyphColor }
                        Rectangle { width: 20; height: 3; radius: 1.5; color: settingsButton.glyphColor }
                        Rectangle { width: 20; height: 3; radius: 1.5; color: settingsButton.glyphColor }
                    }
                }

                ToolTip.visible: hovered
                ToolTip.text: qsTr("Menu")

                onClicked: {
                    topDrawer.close()
                    settingsDrawer.open()
                }
            }

            // Reserved slot for the existing screenshot action which remains a
            // top-level overlay so its screenshot timing/behavior is unchanged.
            Item { Layout.preferredWidth: 48; Layout.preferredHeight: 48 }

            ColumnLayout {
                Layout.alignment: Qt.AlignVCenter
                spacing: 0
                visible: mainPage.width >= 1500

                Label {
                    text: "iScanMR10"
                    color: hmiTheme.text
                    font.pixelSize: 17
                    font.bold: true
                }

                Label {
                    text: "RF / DF OPERATION CONSOLE"
                    color: hmiTheme.muted
                    font.pixelSize: 9
                    font.bold: true
                    font.letterSpacing: 0.8
                }
            }

            HmiStatusPill {
                visible: mainPage.width >= 1500
                darkMode: mainPage.darkTheme
                text: mainPage.operationMode
                tone: mainPage.operationMode === "LOCAL" ? "good" : "warn"
                Layout.alignment: Qt.AlignVCenter
            }

            Item { Layout.fillWidth: true }

            ToolButton {
                id: themeButton
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                Layout.alignment: Qt.AlignVCenter
                hoverEnabled: true

                background: Rectangle {
                    radius: height / 2
                    color: themeButton.hovered ? hmiTheme.card : hmiTheme.cardAlt
                    border.width: 1
                    border.color: mainPage.darkTheme ? hmiTheme.warning : hmiTheme.lineStrong
                }

                contentItem: Text {
                    text: mainPage.darkTheme ? "☀" : "☾"
                    color: mainPage.darkTheme ? hmiTheme.warning : hmiTheme.textSecondary
                    font.pixelSize: 20
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                ToolTip.visible: hovered
                ToolTip.text: mainPage.darkTheme ? qsTr("Light theme") : qsTr("Dark theme")
                onClicked: mainPage.darkTheme = !mainPage.darkTheme
            }

            ColumnLayout {
                id: gpsColumn
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                Layout.maximumWidth: Math.max(180, mainPage.width - (mainPage.compactShell ? 190 : 430))
                spacing: 2

                Label {
                    id: locationLabel
                    visible: mainPage.width >= 1120
                    text:
                        "Latitude "  + Number(navBar.gpsLat).toFixed(6)  + "°N " +
                        "Longitude " + Number(navBar.gpsLong).toFixed(6) + "°E " +
                        "Altitude "  + Number(navBar.gpsAlt).toFixed(2)  + "m"
                    color: hmiTheme.accent
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
                        visible: mainPage.width >= 1480
                        text: "MGRS: " + navBar.formatMGRS(navBar.mgrsText) +
                              "   UTM: " + navBar.formatUTM(navBar.utmText)
                        color: hmiTheme.accent
                        font.pixelSize: 17
                        horizontalAlignment: Text.AlignRight
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Label {
                        id: timeLabel
                        text: navBar.gpsTimeText && navBar.gpsTimeText.length ? navBar.gpsTimeText : "--:--:--"
                        color: hmiTheme.accentHover
                        font.pixelSize: 17
                    }

                    Label {
                        id: dateLabel
                        visible: mainPage.width >= 940
                        text: navBar.gpsDateText && navBar.gpsDateText.length ? navBar.gpsDateText : "---- -- --"
                        color: hmiTheme.accentHover
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
            visible: mainPage.topNetworkDrawerEnabled
            enabled: mainPage.topNetworkDrawerEnabled

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
                    if (!mainPage.topNetworkDrawerEnabled)
                        return

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
                    if (!mainPage.topNetworkDrawerEnabled) return
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

    // ================================ PRIMARY NAVIGATION ================================
    // New HMI shell rail. Existing SideSettingsDrawer stays available from the
    // menu button for local/remote and advanced settings, while primary page
    // navigation is always one touch away.
    Rectangle {
        id: primaryNavigation
        anchors.left: parent.left
        anchors.top: navBar.bottom
        anchors.bottom: parent.bottom
        visible: mainPage.primaryNavigationEnabled
        width: mainPage.primaryNavigationEnabled
               ? (mainPage.narrowShell ? hmiTheme.compactNavigationRailWidth : hmiTheme.navigationRailWidth)
               : 0
        color: hmiTheme.sidebar
        border.color: hmiTheme.line
        border.width: 1
        z: 2

        Column {
            anchors.fill: parent
            anchors.topMargin: 10
            anchors.leftMargin: 6
            anchors.rightMargin: 6
            spacing: 5

            Repeater {
                model: settingsDrawer ? settingsDrawer.toolbarPages() : []

                HmiNavButton {
                    width: primaryNavigation.width - 12
                    darkMode: mainPage.darkTheme
                    label: modelData.title
                    iconSource: mainPage.darkTheme
                                ? (modelData.iconDark || modelData.icon || "")
                                : (modelData.iconLight || modelData.icon || "")
                    active: mainPage.currentPageSource === modelData.source

                    onClicked: {
                        settingsDrawer.requestToolbarNavigation(index,
                                                                modelData.title,
                                                                modelData.source)
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
            navBar.applyBackendClock(currentTime, currentDate, uptime)
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
        anchors.leftMargin: 88

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
        darkMode: mainPage.darkTheme
        krakenmapval: Krakenmapval
        width: Math.min(500, Math.max(320, parent.width * 0.82))
        height: parent.height
        // Phase 8: this drawer owns a full-screen dim/blocker layer.  Keep the
        // whole component above the HMI navigation rail and screenshot button
        // so clicks cannot leak through while it is open, but below the
        // application-level PopupSettingDrawer (z: 10000).
        z: 9000

        onNavigate: function(title, source, index) {
            console.info("[NAV] title=", title, "source=", source, "index=", index)
            if (source !== "qrc:/HomeDisplay.qml") {
                if (loader.depth > 1) loader.pop()
                if (source === "qrc:/Setting.qml") {
                    // NET-AUTH3: every fresh entry to Network Settings starts
                    // in Viewer mode. Admin elevation happens inside Setting.qml.
                    loader.push(source, {
                                    "networkAccessRole": "viewer"
                                })
                } else {
                    loader.push(source)
                }
            } else {
                while (loader.depth > 1) loader.pop()
            }
            currentPageSource = source
            pageSelectorProxy.currentText = title
        }
    }

    PopupSettingDrawer {
        id: popupSetting
        z: 10000
        visible: false
        krakenmapval: Krakenmapval
        darkMode: mainPage.darkTheme

        // ===== Responsive size =====
        // อิง design 1920x1080 (เหมือนหน้าอื่น)
        readonly property real designW: 1920
        readonly property real designH: 1080
        readonly property real uiScale: Math.max(0.6, Math.min(parent.width / designW, parent.height / designH))
        function dp(v) { return Math.round(v * uiScale) }
        function clamp(v, mn, mx) { return Math.max(mn, Math.min(mx, v)) }

        // target size (จากเดิม 1100x600)
        readonly property int targetW: dp(1100)
        readonly property int targetH: dp(600)

        // จำกัดไม่ให้ล้นจอ + เว้นขอบ
        width:  clamp(targetW, dp(520), parent.width  - dp(40))
        height: clamp(targetH, dp(360), parent.height - dp(80))

        // ===== Placement =====
        // วางกลางจอเสมอ (ไม่ต้องใช้ margin ติดลบ)
        anchors.centerIn: parent

        // ถ้าคุณอยากให้ยึดกับ loader แต่ยัง responsive:
        // anchors.centerIn: loader

        // กันหลุดขอบเวลาจอเล็กมาก
        // (Popup บางตัวมี shadow/rounded ต้องเว้นขอบ)
        anchors.margins: dp(10)
    }

    // Phase 10 / V9: outside-click blocker for app popups.
    // IMPORTANT: the click-close MouseAreas are split into four regions around
    // popupSetting.  A full-screen MouseArea below a non-mouse-accepting popup
    // can still steal blank/form clicks, which prevents TextField/SpinBox input
    // and closes Device Parameters immediately.
    Item {
        id: popupSettingBackdrop
        anchors.fill: parent
        visible: popupSetting.visible
        enabled: visible
        z: 9999

        Rectangle {
            anchors.fill: parent
            color: mainPage.darkTheme ? "#000000" : "#E5E8EC"
            opacity: mainPage.darkTheme ? 0.42 : 0.36
        }

        function closePopupFromBackdrop(mouse) {
            popupSetting.close()
            mouse.accepted = true
        }

        // Top outside area
        MouseArea {
            x: 0
            y: 0
            width: parent.width
            height: Math.max(0, popupSetting.y)
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
            preventStealing: true
            propagateComposedEvents: false
            onPressed: popupSettingBackdrop.closePopupFromBackdrop(mouse)
        }

        // Bottom outside area
        MouseArea {
            x: 0
            y: Math.min(parent.height, popupSetting.y + popupSetting.height)
            width: parent.width
            height: Math.max(0, parent.height - y)
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
            preventStealing: true
            propagateComposedEvents: false
            onPressed: popupSettingBackdrop.closePopupFromBackdrop(mouse)
        }

        // Left outside area
        MouseArea {
            x: 0
            y: Math.max(0, popupSetting.y)
            width: Math.max(0, popupSetting.x)
            height: Math.max(0, Math.min(parent.height, popupSetting.y + popupSetting.height) - y)
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
            preventStealing: true
            propagateComposedEvents: false
            onPressed: popupSettingBackdrop.closePopupFromBackdrop(mouse)
        }

        // Right outside area
        MouseArea {
            x: Math.min(parent.width, popupSetting.x + popupSetting.width)
            y: Math.max(0, popupSetting.y)
            width: Math.max(0, parent.width - x)
            height: Math.max(0, Math.min(parent.height, popupSetting.y + popupSetting.height) - y)
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
            preventStealing: true
            propagateComposedEvents: false
            onPressed: popupSettingBackdrop.closePopupFromBackdrop(mouse)
        }
    }


    TopNetworkDrawer {
        id: topDrawer
        krakenmapval: Krakenmapval
        keyfreqEdit: mainPage.keyfreqEdit
        enabled: mainPage.topNetworkDrawerEnabled
        interactive: mainPage.topNetworkDrawerEnabled
    }

    ModePopup {
        id: remoteModePopup
        popupEnabled: mainPage.remoteModePopupEnabled
    }
}
