import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
// import QtGraphicalEffects 1.15

Item {
    id: root
    width: 40
    height: 200

    property int levelmax: 255
    property int levelmin: 128
    property int inivalue: 128
    property alias slider: slider
    property alias progressBar: progressBar

    Slider {
        id: slider
        anchors.fill: parent
        orientation: Qt.Vertical
        from: levelmin
        to: levelmax
        stepSize: 1
        value: inivalue

        // Custom contentItem for styling
        contentItem: Rectangle {
            width: parent.width
            height: parent.height
            color: "transparent"

            Rectangle {
                id: groove
                anchors.centerIn: parent
                width: 8
                height: parent.height
                color: "transparent"
                radius: 4
            }

            Rectangle {
                id: handle
                width: 20
                height: 20
                radius: 10
                color: "transparent"
                y: groove.y + groove.height - slider.visualPosition * groove.height - height / 2
                x: groove.x + groove.width / 2 - width / 2
            }
        }

        background: null
        handle: null
    }

    MyProgressBarVer
    {
        id :progressBar
        maxValue: levelmax
        value: 0
        minValue: levelmin
        anchors.fill: parent
    }
}
