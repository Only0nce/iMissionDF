// PopupSettingDrawer.qml  (FULL FILE - Responsive)
import QtQuick 2.15
import QtGraphicalEffects 1.12

Rectangle {
    id: popuppanel

    // ===== Public API =====
    property var krakenmapval: null

    // ===== Visible / Focus =====
    visible: true
    z: 10000
    focus: visible

    // ===== Content control =====
    property string contentType: ""
    property url pageUrl: ""

    // =========================================================
    // Responsive Metrics
    // =========================================================
    // อิง design เดิม: 1920x1080 และ size เดิม: 1100x600
    readonly property real designW: 1920
    readonly property real designH: 1080

    // scale ตามจอ (กันเล็กเกิน/ใหญ่เกิน)
    readonly property real uiScale: Math.max(0.60, Math.min(parent ? parent.width / designW : 1.0,
                                                          parent ? parent.height / designH : 1.0))

    function dp(v) { return Math.round(v * uiScale) }
    function clamp(v, mn, mx) { return Math.max(mn, Math.min(mx, v)) }

    // ขอบเว้นรอบ popup (กันชนขอบจอ)
    readonly property int safeOuterMargin: dp(20)

    // target size (เดิม)
    readonly property int targetW: dp(1100)
    readonly property int targetH: dp(600)

    // จำกัดไม่ให้ล้นจอ
    width:  parent
            ? clamp(targetW, dp(520), parent.width - safeOuterMargin * 2)
            : 1100

    height: parent
            ? clamp(targetH, dp(360), parent.height - safeOuterMargin * 2)
            : 600

    // วางกลางจอเสมอ (ไม่ต้อง anchor margin ติดลบ)
    anchors.centerIn: parent

    // =========================================================
    // Visual Style
    // =========================================================
    radius: dp(14)
    color: "#111212"
    border.color: "#111212"
    border.width: 1

    // =========================================================
    // Navigation
    // =========================================================
    function urlFor(type) {
        switch (type) {
        case "Group Management":    return "qrc:/iScreenDFqml/popuppanels/GroupSettingPage.qml"
        case "User Setting":        return "qrc:/iScreenDFqml/popuppanels/UserSettingPage.qml"
        case "Setting Parameter":   return "qrc:/iScreenDFqml/popuppanels/SettingParameter.qml"
        case "Add Device":          return "qrc:/iScreenDFqml/popuppanels/AddDevicePage.qml"
        default:                    return "qrc:/iScreenDFqml/popuppanels/DefaultPage.qml"
        }
    }

    function openWithMessage(msg) {
        contentType = msg
        pageUrl = urlFor(msg)

        // ส่ง krakenmapval ให้หน้าลูก
        contentLoader.setSource(pageUrl, { krakenmapval: popuppanel.krakenmapval })

        console.log("[PopupSettingDrawer] show content:", msg, "->", pageUrl)
        open()
    }

    onKrakenmapvalChanged: {
        if (contentLoader.item && contentLoader.item.hasOwnProperty("krakenmapval")) {
            contentLoader.item.krakenmapval = krakenmapval
        }
    }

    function open() {
        visible = true
        forceActiveFocus()
    }

    function close() {
        visible = false
        closeDelay.restart()
    }

    function toggle() { visible ? close() : open() }

    Keys.onReleased: {
        if (event.key === Qt.Key_Escape) {
            close()
            event.accepted = true
        }
    }

    // =========================================================
    // States & Animations
    // =========================================================
    states: [
        State {
            name: "hidden"
            when: !popuppanel.visible
            PropertyChanges { target: popuppanel; opacity: 0.0; scale: 0.96 }
        },
        State {
            name: "shown"
            when: popuppanel.visible
            PropertyChanges { target: popuppanel; opacity: 1.0; scale: 1.0 }
        }
    ]

    transitions: [
        Transition {
            from: "hidden"; to: "shown"
            NumberAnimation { properties: "opacity,scale"; duration: 180; easing.type: Easing.InOutQuad }
        },
        Transition {
            from: "shown"; to: "hidden"
            NumberAnimation { properties: "opacity,scale"; duration: 180; easing.type: Easing.InOutQuad }
        }
    ]

    Timer {
        id: closeDelay
        interval: 200
        repeat: false
    }

    onVisibleChanged: {
        if (visible) { opacity = 1.0; scale = 1.0 }
    }

    // =========================================================
    // Content
    // =========================================================
    Loader {
        id: contentLoader
        anchors.fill: parent

        // margin responsive (เดิม 20)
        anchors.margins: dp(20)
        anchors.bottomMargin: dp(20)

        asynchronous: false
        source: pageUrl

        onStatusChanged: {
            if (status === Loader.Ready && item) {
                // hook optional
            }
        }
    }
    // Row {
    //     id: actionRow
    //     spacing: 20
    //     anchors.bottom: parent.bottom
    //     anchors.right: parent.right
    //     anchors.bottomMargin: 0
    //     anchors.rightMargin: 0

    //     CancelButtonPopupSettingDrawer {
    //         anchors.right: parent.right
    //         anchors.bottom: parent.bottom
    //         anchors.rightMargin: 105
    //         anchors.bottomMargin: 20
    //         onClicked: popuppanel.close()
    //     }
    //     ApplyButtonPopupSettingDrawer {
    //         anchors.right: parent.right
    //         anchors.bottom: parent.bottom
    //         anchors.rightMargin: 20
    //         anchors.bottomMargin: 20
    //         onClicked: {
    //             console.log("APPLY clicked")
    //             popuppanel.close()
    //         }
    //     }
        // BackButtonPopupSettingDrawer {
        //     anchors.right: parent.right
        //     anchors.bottom: parent.bottom
        //     anchors.rightMargin: 1000
        //     anchors.bottomMargin: 20
        //     onClicked: popuppanel.close()
        // }
    // }
}
