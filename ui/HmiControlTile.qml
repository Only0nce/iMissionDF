import QtQuick 2.15

Rectangle {
    id: root

    property bool darkMode: true
    property string text: "CONTROL"
    property string tone: "primary" // primary | neutral | warning | danger | good
    property bool active: false
    property bool hovered: false
    property bool pressed: false
    property int fontPixelSize: 12
    // User requested higher contrast for the green control tiles.
    // Keep neutral tiles on theme.text, but use a bright white foreground on
    // all colored control tiles in both dark and light themes.
    property color textColor: "#F7FFFE"

    Theme { id: theme; darkMode: root.darkMode }

    radius: theme.radiusSm
    border.width: active || hovered ? 2 : 1
    border.color: active
                  ? theme.accentHover
                  : (hovered ? theme.accentHover : theme.lineStrong)

    function baseColor() {
        switch (tone) {
        case "danger": return theme.danger
        case "warning": return theme.warning
        case "good": return theme.success
        case "neutral": return theme.cardAlt
        default: return theme.accent
        }
    }

    function currentColor() {
        var c = baseColor()
        if (pressed)
            return Qt.darker(c, 1.18)
        if (active || hovered)
            return Qt.lighter(c, 1.08)
        return c
    }

    color: currentColor()

    Behavior on color { ColorAnimation { duration: 100 } }
    Behavior on border.color { ColorAnimation { duration: 100 } }

    Text {
        anchors.fill: parent
        anchors.margins: 4
        text: root.text
        color: root.tone === "neutral" ? theme.text : root.textColor
        font.pixelSize: root.fontPixelSize
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
        maximumLineCount: 2
        elide: Text.ElideRight
    }
}
