import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
// import QtGraphicalEffects 1.15

Item {
    id: root
    width: 40
    height: 200

    property int levelmax: 100
    property int levelmin: 0
    property int inivalue: 50
    property alias slider2: slider2
    property alias progressBar2: progressBar2

    Slider {
        id: slider2
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
                id: groove2
                anchors.centerIn: parent
                width: 8
                height: parent.height
                color: "transparent"
                radius: 4
            }

            Rectangle {
                id: handle2
                width: 20
                height: 20
                radius: 10
                color: "transparent"
                y: groove2.y + groove2.height - slider2.visualPosition * groove2.height - height / 2
                x: groove2.x + groove2.width / 2 - width / 2
            }
        }

        background: null
        handle: null
    }

    MyProgressBarVer
    {
        id :progressBar2
        maxValue: levelmax
        value: 0
        minValue: levelmin
        anchors.fill: parent
    }
}
