import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: row
    width: parent ? parent.width : 500
    height: 52
    radius: 8
    color: "#10171f"
    border.color: "#223045"

    // public props
    property string devName: ""
    property bool online: true

    readonly property color text: "#eeeeee"
    readonly property color ok  : "#2ecc71"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        Row {
            spacing: 2
            Repeater {
                model: 3
                Rectangle {
                    width: 6; height: (index+1)*6 + 6
                    color: row.online ? ok : "#6b7a8a"; radius: 2
                    anchors.bottom: parent.bottom
                }
            }
            Layout.alignment: Qt.AlignVCenter
        }

        ColumnLayout {
            Layout.fillWidth: true; spacing: 2
            Label { text: row.devName; color: text; font.pixelSize: 14 }
            Label { text: row.online ? "Online" : "Offline"
                    color: row.online ? ok : "#ff6b6b"; font.pixelSize: 12 }
        }

        ToolButton {
            text: "\u2699" // ⚙
            contentItem: Text { text: parent.text; color: text; font.pixelSize: 16
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter }
            onClicked: console.log("settings for", row.devName)
        }
        ToolButton {
            text: "\u22EE" // ⋮
            contentItem: Text { text: parent.text; color: text; font.pixelSize: 18
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter }
            onClicked: console.log("menu for", row.devName)
        }
    }
}
