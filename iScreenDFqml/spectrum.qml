// spectrum.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import QtQuick.Controls.Material 2.15

Item {
    id: spectrumPage
    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: "#1e1e1e"

        Label {
            anchors.centerIn: parent
            text: "SPECTRUM PAGE"
            color: "#ffffff"
            font.pixelSize: 32
            font.bold: true
        }
    }
}
