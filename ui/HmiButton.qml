import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
    id: root

    property bool darkMode: true
    property string tone: "normal" // normal | primary | danger | warning
    property bool compact: false
    property int fontPixelSize: compact ? 11 : 12

    Theme { id: theme; darkMode: root.darkMode }

    implicitHeight: compact ? theme.minTouchTarget : theme.controlHeight
    implicitWidth: Math.max(compact ? 72 : 88, contentItem.implicitWidth + 24)
    hoverEnabled: true

    function fillColor() {
        if (!enabled)
            return theme.disabled
        if (tone === "primary")
            return pressed ? Qt.darker(theme.accent, 1.18) : theme.accent
        if (tone === "danger")
            return pressed ? Qt.darker(theme.danger, 1.18) : theme.danger
        if (tone === "warning")
            return pressed ? Qt.darker(theme.warning, 1.18) : theme.warning
        return pressed ? theme.cardAlt : (hovered ? theme.card : theme.input)
    }

    background: Rectangle {
        radius: theme.radiusSm
        color: root.fillColor()
        border.width: 1
        border.color: root.tone === "normal"
                      ? (root.hovered ? theme.accentHover : theme.lineStrong)
                      : root.fillColor()
        Behavior on color { ColorAnimation { duration: 100 } }
    }

    contentItem: Text {
        text: root.text
        color: !root.enabled
               ? theme.muted
               : (root.tone === "normal" ? theme.text : "#061514")
        font.pixelSize: root.fontPixelSize
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
