// popuppanels/UserSettingPage.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: usersettingpage
    anchors.fill: parent
    property var krakenmapval: null

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        Label {
            text: "USER SETTINGS PAGE"
            color: "white"
            font.pixelSize: 18
            font.bold: true
        }

        TextField {
            id: username
            placeholderText: "Username"
            Layout.preferredWidth: 240
        }

        Button {
            text: "Save"
            onClicked: console.log("[UserSettingPage] Save:", username.text)
        }
    }
}
