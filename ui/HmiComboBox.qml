import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15

ComboBox {
    id: root

    property bool darkMode: Material.theme === Material.Dark
    property bool compact: false
    property int fontPixelSize: compact ? 11 : 12
    property int popupMaxHeight: 320
    property int popupMinWidth: 0

    Theme { id: theme; darkMode: root.darkMode }

    Material.theme: root.darkMode ? Material.Dark : Material.Light
    Material.background: theme.panel
    Material.foreground: theme.text
    Material.primary: theme.accent
    Material.accent: theme.accent

    font.pixelSize: root.fontPixelSize
    implicitHeight: compact ? theme.minTouchTarget : theme.controlHeight
    implicitWidth: Math.max(96, contentItem.implicitWidth + 46)
    leftPadding: 10
    rightPadding: 34
    topPadding: 0
    bottomPadding: 0

    contentItem: Text {
        text: root.displayText
        color: root.enabled ? theme.text : theme.muted
        font.pixelSize: root.font.pixelSize
        font.bold: false
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignLeft
        elide: Text.ElideRight
    }

    indicator: Item {
        width: 28
        height: root.height
        x: root.width - width - 2
        y: 0

        Text {
            anchors.centerIn: parent
            text: root.popup.visible ? "▴" : "▾"
            color: root.enabled ? theme.textSecondary : theme.muted
            font.pixelSize: 13
            font.bold: true
        }
    }

    background: Rectangle {
        radius: theme.radiusSm
        color: root.enabled ? theme.input : theme.cardAlt
        border.width: root.activeFocus || root.popup.visible ? 2 : 1
        border.color: root.activeFocus || root.popup.visible
                      ? theme.accent
                      : (root.enabled ? theme.lineStrong : theme.line)
        Behavior on border.color { ColorAnimation { duration: 90 } }
    }

    delegate: ItemDelegate {
        id: delegateItem
        width: root.popup.width - 2
        height: Math.max(38, theme.minTouchTarget)
        hoverEnabled: true
        leftPadding: 10
        rightPadding: 8
        highlighted: root.highlightedIndex === index

        contentItem: Text {
            text: root.textAt(index)
            color: delegateItem.enabled ? theme.text : theme.muted
            font.pixelSize: root.font.pixelSize
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: theme.radiusXs
            color: delegateItem.highlighted
                   ? (root.darkMode ? Qt.rgba(0.07, 0.64, 0.53, 0.28)
                                    : Qt.rgba(0.00, 0.55, 0.46, 0.13))
                   : (delegateItem.hovered ? theme.cardAlt : theme.panel)
            border.width: root.currentIndex === index ? 1 : 0
            border.color: theme.accent
        }
    }

    popup: Popup {
        Material.theme: root.darkMode ? Material.Dark : Material.Light
        Material.background: theme.panel
        Material.foreground: theme.text
        Material.primary: theme.accent
        Material.accent: theme.accent
        y: root.height + 4
        width: Math.max(root.width, root.popupMinWidth)
        implicitHeight: Math.min(contentItem.implicitHeight + 12, root.popupMaxHeight)
        padding: 6
        margins: 8

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
            ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: Rectangle {
            radius: theme.radiusMd
            color: theme.panel
            border.width: 1
            border.color: theme.lineStrong
        }
    }
}
