import QtQuick 2.12
import QtQuick.Controls 2.12

Item {
    id: root
    width: parent ? parent.width : 1920
    height: parent ? parent.height : 1080
    clip: true

    property string initialNetworkPage: "wifi"
    property bool forceSingleNetworkPage: false
    property bool hideInternalNetworkTabs: false

    // KP-6JUL2026 : Explicit host-page scope prevents a 5G page instance
    // from briefly starting WiFi scans while its initial page is being assigned.
    property string pageScope: initialNetworkPage

    signal requestToast(string text)

    Loader {
        id: pageLoader
        anchors.fill: parent
        source: "qrc:/Wifi5GPage.qml"
        asynchronous: true

        onLoaded: {
            if (!item)
                return
            item.pageScope = root.pageScope
            item.initialNetworkPage = root.initialNetworkPage
            item.forceSingleNetworkPage = root.forceSingleNetworkPage
            item.hideInternalNetworkTabs = root.hideInternalNetworkTabs
        }
    }

    Connections {
        target: pageLoader.item
        ignoreUnknownSignals: true
        function onRequestToast(text) { root.requestToast(text) }
    }

    onPageScopeChanged: if (pageLoader.item) pageLoader.item.pageScope = pageScope
    onInitialNetworkPageChanged: if (pageLoader.item) pageLoader.item.initialNetworkPage = initialNetworkPage
    onForceSingleNetworkPageChanged: if (pageLoader.item) pageLoader.item.forceSingleNetworkPage = forceSingleNetworkPage
    onHideInternalNetworkTabsChanged: if (pageLoader.item) pageLoader.item.hideInternalNetworkTabs = hideInternalNetworkTabs

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 80, 900)
        height: 260
        radius: 18
        visible: pageLoader.status === Loader.Error
        color: "#132235"
        border.color: "#2d4056"

        Text {
            x: 32
            y: 30
            text: "Wireless page failed to load"
            color: "#e9f0f7"
            font.pixelSize: 28
            font.bold: true
        }
        Text {
            x: 32
            y: 86
            width: parent.width - 64
            text: "Wifi5GPage.qml or Wifi5GView.qml returned Loader.Error. Run from terminal and check QML console output."
            color: "#9aa8b8"
            font.pixelSize: 15
            wrapMode: Text.WordWrap
        }
        Rectangle {
            x: 32
            y: 160
            width: parent.width - 64
            height: 56
            radius: 8
            color: "#0d1723"
            border.color: "#2d4056"
            Text {
                anchors.centerIn: parent
                text: "QT_LOGGING_RULES=\"qt.qml.*=true;qt.quick.*=true\" ./your_app_name"
                color: "#00c9a7"
                font.pixelSize: 14
            }
        }
    }
}
