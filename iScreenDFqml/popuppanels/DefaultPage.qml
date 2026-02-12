// popuppanels/DefaultPage.qml
import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: defaultpage
    anchors.fill: parent
    property var krakenmapval: null

    Label {
        anchors.centerIn: parent
        text: "UNKNOWN PAGE"
        color: "white"
        font.pixelSize: 18
    }
}
