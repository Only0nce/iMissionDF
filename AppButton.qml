import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
    id: root

    property color baseColor: "#2fa6ff"
    property color textColor: "white"
    property int buttonHeight: 42
    property int buttonRadius: 8
    property int buttonFontSize: 14
    property bool busy: false
    property int busyIndicatorSize: 18

    implicitHeight: buttonHeight

    contentItem: Item {
        Row {
            anchors.centerIn: parent
            spacing: 6

            BusyIndicator {
                visible: root.busy
                running: root.busy
                width: root.busyIndicatorSize
                height: root.busyIndicatorSize
            }

            Text {
                text: root.text
                color: root.textColor
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: root.buttonFontSize
                font.bold: true
                elide: Text.ElideRight
            }
        }
    }

    background: Rectangle {
        radius: root.buttonRadius
        color: root.enabled ? root.baseColor : "#354052"
        opacity: root.down ? 0.75 : 1.0
        border.color: Qt.lighter(color, 1.15)
        border.width: 1
    }
}
