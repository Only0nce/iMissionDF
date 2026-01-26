import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
import QtQuick.Controls.Material 2.4
import QtQuick.Layouts 1.0

Item {
    id: _item
    property alias buttonIn: buttonIn
    property alias buttonOut: buttonOut
    property alias buttonReset: buttonReset
    property alias buttonClear: buttonClear
    width: 60
    height: 150
    Rectangle {
        id: rectangle
        color: "#00000000"
        radius: 5
        anchors.fill: parent


        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            anchors.leftMargin: 0
            anchors.rightMargin: 0
            anchors.topMargin: 0
            anchors.bottomMargin: 0
            spacing: 0
            z: 98



            ToolButton {
                id: buttonIn
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 75
                Layout.preferredHeight: 95

                Image {
                    anchors.fill: parent
                    source: "images/zoomin.png"
                    fillMode: Image.PreserveAspectFit
                    sourceSize.height: 40
                    sourceSize.width: 40
                }

            }

            ToolButton {
                id: buttonOut
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 75
                Layout.preferredHeight: 95

                Image {
                    anchors.fill: parent
                    anchors.topMargin: 3
                    source: "images/zoomout.png"
                    fillMode: Image.PreserveAspectFit
                    sourceSize.height: 40
                    sourceSize.width: 40
                }
            }


            ToolButton {
                id: buttonReset
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 75
                Layout.preferredHeight: 95

                Image {
                    anchors.fill: parent
                    source: "images/zoomreset.png"
                    fillMode: Image.PreserveAspectFit
                    sourceSize.height: 40
                    sourceSize.width: 40
                }

            }

            ToolButton {
                id: buttonClear
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 75
                Layout.preferredHeight: 95

                Image {
                    anchors.fill: parent
                    anchors.topMargin: 4
                    anchors.bottomMargin: 4
                    source: "images/rotate-right.png"
                    fillMode: Image.PreserveAspectFit
                }

            }
        }
    }

}
