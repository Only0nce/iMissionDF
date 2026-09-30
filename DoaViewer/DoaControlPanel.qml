import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../ui"

HmiPanel {
    id: root
    clip: true
    implicitWidth: 188
    Theme { id: theme; darkMode: root.darkMode }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 6

        ColumnLayout {
            spacing: 1
            Layout.preferredWidth: 58
            Text {
                text: "DOA"
                color: theme.text
                font.pixelSize: 12
                font.bold: true
            }
            Text {
                text: "ENGINE"
                color: theme.muted
                font.pixelSize: 8
                font.bold: true
                font.letterSpacing: 0.8
            }
        }

        Switch {
            checked: doaClient.doaEnabled
            enabled: doaClient.connected
            onToggled: doaClient.doaEnabled = checked
        }

        HmiStatusPill {
            darkMode: root.darkMode
            text: doaClient.doaEnabled ? "ON" : "OFF"
            tone: doaClient.doaEnabled ? "good" : "danger"
            compact: true
        }

        Item { Layout.fillWidth: true }
    }
}
