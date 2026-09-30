import QtQuick 2.15
import QtQuick.Controls 2.15

ToolButton {
    id: root

    property bool darkMode: true
    property bool active: false
    property string label: "PAGE"
    property url iconSource: ""

    Theme { id: theme; darkMode: root.darkMode }

    width: 64
    height: 68
    hoverEnabled: true

    background: Rectangle {
        radius: theme.radiusMd
        color: root.active
               ? theme.navTileActive
               : (root.hovered ? theme.navTileHover : theme.navTile)
        border.width: 1
        border.color: root.active ? theme.remoteRowBorderActive : theme.navTileBorder

        Behavior on color { ColorAnimation { duration: 120 } }
    }

    contentItem: Column {
        anchors.centerIn: parent
        spacing: 4

        Image {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 25
            height: 25
            source: root.iconSource
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            opacity: root.active ? 1.0 : theme.navIconOpacity
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            width: root.width - 8
            text: root.label.replace("\n", " ")
            color: root.active ? theme.navTileActiveText : theme.navTileText
            font.pixelSize: 9
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }
    }

    ToolTip.visible: hovered
    ToolTip.text: label.replace("\n", " ")
}
