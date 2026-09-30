import QtQuick 2.15

Rectangle {
    id: root

    property bool darkMode: true
    property string label: "METRIC"
    property string value: "--"
    property string tone: "neutral" // neutral | good | warn | danger | info
    property bool compact: true

    Theme { id: theme; darkMode: root.darkMode }

    function toneColor() {
        switch (tone) {
        case "good": return theme.success
        case "warn": return theme.warning
        case "danger": return theme.danger
        case "info": return theme.info
        default: return theme.accentHover
        }
    }

    implicitHeight: compact ? 28 : 36
    implicitWidth: content.implicitWidth + 18
    radius: theme.radiusSm
    color: theme.analyzerHud
    border.width: 1
    border.color: theme.analyzerBorder

    Row {
        id: content
        anchors.centerIn: parent
        spacing: 6

        Text {
            text: root.label
            color: theme.analyzerMuted
            font.pixelSize: root.compact ? 9 : 10
            font.bold: true
            font.letterSpacing: 0.4
        }

        Text {
            text: root.value
            color: root.tone === "neutral" ? theme.analyzerText : root.toneColor()
            font.pixelSize: root.compact ? 10 : 12
            font.bold: true
            font.family: "monospace"
        }
    }
}
