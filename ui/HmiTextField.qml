import QtQuick 2.15
import QtQuick.Controls 2.15

TextField {
    id: root

    property bool darkMode: true
    property bool compact: false
    property bool emphasized: false
    property int fontPixelSize: compact ? 11 : 12

    Theme { id: theme; darkMode: root.darkMode }

    implicitHeight: compact ? theme.minTouchTarget : theme.controlHeight
    color: root.enabled ? (root.emphasized ? theme.accentHover : theme.text) : theme.muted
    placeholderTextColor: theme.muted
    selectionColor: theme.accent
    selectedTextColor: "#061514"
    font.pixelSize: root.fontPixelSize
    font.bold: emphasized
    leftPadding: 10
    rightPadding: 10
    topPadding: 0
    bottomPadding: 0
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true

    background: Rectangle {
        radius: theme.radiusSm
        color: root.enabled ? theme.input : theme.cardAlt
        border.width: root.activeFocus || root.emphasized ? 2 : 1
        border.color: root.activeFocus || root.emphasized
                      ? theme.accent
                      : (root.enabled ? theme.lineStrong : theme.line)
        Behavior on border.color { ColorAnimation { duration: 100 } }
    }
}
