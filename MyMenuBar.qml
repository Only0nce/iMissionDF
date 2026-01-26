import QtQuick 2.15
import Qt.labs.settings 1.1
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
Item {
    id: _item
    width: 75
    height: 400
    property alias toolButtonPower: toolButtonPower

    Rectangle {
        id: rectangle1
        color: "#009688"
        anchors.fill: parent
        radius: 10

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            ToolButton {
                id: toolButtonPower
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.preferredWidth: 75
                Layout.preferredHeight: 95

                Label {
                    text: qsTr("Power")
                    anchors.bottom: parent.bottom
                    font.pointSize: 10
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Image {
                    width: 65
                    anchors.verticalCenter: parent.verticalCenter
                    source: "images/powerButton.png"
                    anchors.verticalCenterOffset: -10
                    anchors.horizontalCenter: parent.horizontalCenter
                    fillMode: Image.PreserveAspectFit
                }
            }
        }
    }
}
