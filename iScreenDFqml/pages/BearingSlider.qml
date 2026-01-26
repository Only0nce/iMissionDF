import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.15

Item {
    id: bearingSlider
    width: 60
    height: 400
    opacity: active ? 1.0 : 0.15
    // opacity : 1.0
    property var mapRef
    property int bearingMin: 0
    property int bearingMax: 360
    property real dragBearingValue: 0
    property bool active: false

    Behavior on opacity {
        NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
    }

    Timer {
        id: fadeTimer
        interval: 3000
        repeat: false
        onTriggered: active = false
    }

    function triggerFadeIn() {
        active = true
        fadeTimer.restart()
    }

    Column {
        anchors.fill: parent
        spacing: 8

        Button {
            width: 28
            height: 28
            anchors.horizontalCenter: parent.horizontalCenter

            onClicked: {
                dragBearingValue = (dragBearingValue + 1) % 360
                mapRef.bearing = dragBearingValue
                triggerFadeIn()
            }

            background: Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: "#169976"
                opacity: 0.9
            }

            Image {
                id: addIcon
                anchors.centerIn: parent
                source: "qrc:/iScreenDFqml/images/addicon.png"    // หรือ "images/add_icon.png"
                width: 16
                height: 16
                fillMode: Image.PreserveAspectFit
            }
        }

        Rectangle {
            id: rotateSliderArea
            width: 40
            height: 300
            radius: 10
            color: "transparent"
            border.color: "#444"
            anchors.horizontalCenter: parent.horizontalCenter
            opacity: 0.6

            Rectangle {
                id: backgroundBlur
                anchors.fill: parent
                radius: 10
                color: "#20232a88"
                z: -2
            }

            FastBlur {
                anchors.fill: backgroundBlur
                source: backgroundBlur
                radius: 32
                transparentBorder: true
                z: -1
            }

            Rectangle {
                id: knob
                width: 36
                height: 22
                radius: 5
                opacity: 0.9
                color: "#222"
                border.color: "#00ffff"
                border.width: 2
                x: 2
                y: (1 - (dragBearingValue / (bearingMax - bearingMin))) * (rotateSliderArea.height - height)

                Text {
                    anchors.centerIn: parent
                    text: Math.round(dragBearingValue) + "°"
                    color: "white"
                    font.pixelSize: 11
                    font.bold: true
                }

                MouseArea {
                    anchors.fill: parent
                    drag.target: parent
                    drag.axis: Drag.YAxis
                    drag.minimumY: 0
                    drag.maximumY: rotateSliderArea.height - height

                    onPressed: triggerFadeIn()

                    onPositionChanged: {
                        triggerFadeIn()
                        const ratio = 1 - (knob.y / (rotateSliderArea.height - knob.height))
                        let val = bearingMin + ratio * (bearingMax - bearingMin)
                        val = Math.round(val)
                        val = Math.max(0, Math.min(360, val))
                        if (Math.round(dragBearingValue) !== val) {
                            dragBearingValue = val
                            mapRef.bearing = (val === 360 ? 0 : val)
                        }
                    }

                    onReleased: {
                        triggerFadeIn()
                        mapRef.bearing = (dragBearingValue === 360 ? 0 : dragBearingValue)
                    }
                }
            }
        }

        Button {
            width: 28
            height: 28
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: {
                dragBearingValue = (dragBearingValue - 1 + 360) % 360
                mapRef.bearing = dragBearingValue
                triggerFadeIn()
            }
            background: Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: "#169976"
                opacity: 0.9
            }

            Image {
                id: subtractIcon
                anchors.centerIn: parent
                source: "qrc:/iScreenDFqml/images/subtracticon.png"    // หรือ "images/add_icon.png"
                width: 16
                height: 16
                fillMode: Image.PreserveAspectFit
            }
        }

        Text {
            text: "rotate map"
            color: "#eeeeee"
            font.pixelSize: 12
            opacity: 0.9
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    Connections {
        target: mapRef
        function onBearingChanged() {
            if (!knob.Drag.active) {
                dragBearingValue = mapRef.bearing
                knob.y = (1 - (dragBearingValue / (bearingMax - bearingMin))) * (rotateSliderArea.height - knob.height)
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        z: -10
        onEntered: triggerFadeIn()
    }
}
