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
    property bool darkMode: true
    readonly property color buttonFill: darkMode ? "#223A42" : "#008B75"
    readonly property color buttonFillHover: darkMode ? "#2F4D57" : "#16A98F"
    readonly property color buttonBorder: darkMode ? "#6FAFBE" : "#00705F"
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
                hoverEnabled: true
                background: Rectangle {
                    radius: 8
                    color: buttonIn.pressed ? Qt.darker(_item.buttonFill, 1.18) : (buttonIn.hovered ? _item.buttonFillHover : _item.buttonFill)
                    border.width: 1
                    border.color: _item.buttonBorder
                    opacity: _item.darkMode ? 0.55 : 0.92
                }
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
                hoverEnabled: true
                background: Rectangle {
                    radius: 8
                    color: buttonOut.pressed ? Qt.darker(_item.buttonFill, 1.18) : (buttonOut.hovered ? _item.buttonFillHover : _item.buttonFill)
                    border.width: 1
                    border.color: _item.buttonBorder
                    opacity: _item.darkMode ? 0.55 : 0.92
                }
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
                hoverEnabled: true
                background: Rectangle {
                    radius: 8
                    color: buttonReset.pressed ? Qt.darker(_item.buttonFill, 1.18) : (buttonReset.hovered ? _item.buttonFillHover : _item.buttonFill)
                    border.width: 1
                    border.color: _item.buttonBorder
                    opacity: _item.darkMode ? 0.55 : 0.92
                }
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
                hoverEnabled: true
                background: Rectangle {
                    radius: 8
                    color: buttonClear.pressed ? Qt.darker(_item.buttonFill, 1.18) : (buttonClear.hovered ? _item.buttonFillHover : _item.buttonFill)
                    border.width: 1
                    border.color: _item.buttonBorder
                    opacity: _item.darkMode ? 0.55 : 0.92
                }
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
