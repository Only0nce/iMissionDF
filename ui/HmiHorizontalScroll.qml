import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// Responsive one-line control strip.  At wide resolutions it behaves like a
// normal RowLayout.  When the preferred controls no longer fit, the strip can
// be panned horizontally without shrinking touch targets or overlapping items.
Flickable {
    id: root

    default property alias contentData: contentRow.data
    property alias spacing: contentRow.spacing
    property bool showScrollBar: contentWidth > width + 1

    clip: true
    flickableDirection: Flickable.HorizontalFlick
    boundsBehavior: Flickable.StopAtBounds
    interactive: contentWidth > width + 1
    contentWidth: Math.max(width, contentRow.implicitWidth)
    contentHeight: height

    function clampContentX() {
        contentX = Math.max(0, Math.min(contentX, Math.max(0, contentWidth - width)))
    }
    onWidthChanged: clampContentX()
    onContentWidthChanged: clampContentX()

    RowLayout {
        id: contentRow
        x: 0
        y: 0
        width: Math.max(root.width, implicitWidth)
        height: root.height
    }

    ScrollBar.horizontal: ScrollBar {
        policy: root.showScrollBar ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
    }
}
