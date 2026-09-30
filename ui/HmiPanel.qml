import QtQuick 2.15

Rectangle {
    id: root
    property bool darkMode: true
    property bool elevated: false

    Theme { id: theme; darkMode: root.darkMode }

    color: theme.panel
    radius: theme.radiusLg
    border.width: 1
    border.color: theme.line
}
