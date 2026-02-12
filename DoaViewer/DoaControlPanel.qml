import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    radius: 10
    color: "#0B1220"
    border.color: "#223049"
    border.width: 1

    RowLayout {
        anchors.fill: parent
        // anchors.margins: 8
        anchors.leftMargin: 8
        spacing: 10

        Text { text: "DOA"; color: "#E5E7EB"; font.pixelSize: 13 }

        Switch {
            checked: doaClient.doaEnabled
            enabled: doaClient.connected
            onToggled: doaClient.doaEnabled = checked
        }

        Text {
            text: doaClient.doaEnabled ? "ON" : "OFF"
            color: doaClient.doaEnabled ? "#22c55e" : "#f87171"
            font.pixelSize: 13
        }
    }
}
