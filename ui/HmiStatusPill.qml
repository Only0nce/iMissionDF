import QtQuick 2.15

Rectangle {
    id: root

    property bool darkMode: true
    property string text: "READY"
    property string tone: "neutral" // neutral | good | warn | danger | info
    property int horizontalPadding: 13
    property bool compact: false

    Theme { id: theme; darkMode: root.darkMode }

    function toneColor() {
        switch (tone) {
        case "good": return theme.success
        case "warn": return theme.warning
        case "danger": return theme.danger
        case "info": return theme.info
        default: return theme.lineStrong
        }
    }

    implicitHeight: compact ? 28 : 34
    implicitWidth: label.implicitWidth + (compact ? Math.max(8, horizontalPadding - 3) : horizontalPadding) * 2
    radius: height / 2
    clip: true
    color: theme.cardAlt
    border.width: 1
    border.color: toneColor()

    Text {
        id: label
        anchors.centerIn: parent
        width: Math.max(0, root.width - 12)
        horizontalAlignment: Text.AlignHCenter
        text: root.text
        color: root.tone === "neutral" ? theme.textSecondary : root.toneColor()
        font.pixelSize: root.compact ? 10 : 12
        font.bold: true
        font.letterSpacing: 0.3
        elide: Text.ElideRight
    }
}
