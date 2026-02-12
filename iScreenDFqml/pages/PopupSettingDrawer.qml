// PopupSettingDrawer.qml
import QtQuick 2.15
import QtGraphicalEffects 1.12

Rectangle {
    id: popuppanel
    width: 1100
    height: 600
    radius: 14
    color: "#111212"
    border.color: "#111212"
    border.width: 1

    // ===== Public API =====
    property var krakenmapval: null

    visible: true
    z: 10000

    property string contentType: ""
    property url pageUrl: ""

    focus: visible

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

    Keys.onReleased: if (event.key === Qt.Key_Escape) close()

    // ============ States & Animations ============
    states: [
        State { name: "hidden"; when: !popuppanel.visible
            PropertyChanges { target: popuppanel; opacity: 0.0; scale: 0.96 } },
        State { name: "shown"; when: popuppanel.visible
            PropertyChanges { target: popuppanel; opacity: 1.0; scale: 1.0 } }
    ]
    transitions: [
        Transition {
            from: "hidden"; to: "shown"
            NumberAnimation { properties: "opacity,scale"; duration: 180; easing.type: Easing.InOutQuad } },
        Transition {
            from: "shown"; to: "hidden"
            NumberAnimation { properties: "opacity,scale"; duration: 180; easing.type: Easing.InOutQuad } }
    ]

    Timer {
        id: closeDelay
        interval: 200
        repeat: false
    }

    onVisibleChanged: {
        if (visible) { opacity = 1.0; scale = 1.0 }
    }

    // ================== เนื้อหา ==================
    Loader {
        id: contentLoader
        anchors.fill: parent
        anchors.margins: 20
        anchors.bottomMargin: 20
        asynchronous: false      // ถ้ากระตุกค่อยเปลี่ยนเป็น true
        source: pageUrl

        // ถ้าต้องเชื่อมสัญญาณจากหน้าเข้ามาที่ popup นี้ ทำตรงนี้ได้
        onStatusChanged: {
            if (status === Loader.Ready && item) {
                // ตัวอย่าง: ถ้าหน้าลูกมี signal ชื่อ done(string msg)
                // if (item.done) item.done.connect(function (m){ console.log("child done:", m) })
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
        BackButtonPopupSettingDrawer {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 1000
            anchors.bottomMargin: 20
            onClicked: popuppanel.close()
        }
    // }
}
