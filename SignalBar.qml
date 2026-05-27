import QtQuick 2.15

Item {
    id: root

    property int value: 0
    property color goodColor: "#00c896"
    property color warningColor: "#f59e0b"
    property color dangerColor: "#ef4444"
    property color inactiveColor: "#273243"
    property color borderColor: "#334155"
    property int barWidth: 150
    property int barHeight: 16
    property int itemWidth: 22
    property int itemSpacing: 4
    property int itemRadius: 4

    implicitWidth: barWidth
    implicitHeight: barHeight

    function levelColor(signalValue) {
        if (signalValue >= 70)
            return goodColor
        if (signalValue >= 40)
            return warningColor
        return dangerColor
    }

    Row {
        anchors.fill: parent
        spacing: root.itemSpacing

        Repeater {
            model: 5

            Rectangle {
                width: root.itemWidth
                height: parent.height
                radius: root.itemRadius
                color: root.value >= ((index + 1) * 20) ? root.levelColor(root.value) : root.inactiveColor
                border.color: root.borderColor
                border.width: 1
            }
        }
    }
}
