// ZoomSlider.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Shapes 1.15
import QtGraphicalEffects 1.15

Item {
    id: zoomslider
    width: 320
    height: 80

    property bool isDarkTheme: true
    property var zoomTarget
    property real zoomMin
    property real zoomMax
    property real zoomStep
    property bool active: false
    opacity: active ? 0.9 : 0.25

    Behavior on opacity {
        NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
    }

    Timer {
        id: fadeTimer
        interval: 1000
        repeat: false
        onTriggered: zoomslider.active = false
    }

    function updateHandlePosition() {
        if (!zoomTarget) return
        handle.x = 40 + ((zoomTarget.zoomLevel - zoomMin) / (zoomMax - zoomMin)) * (zoomslider.width - 80 - handle.width)
    }

    Connections {
        target: zoomTarget
        function onZoomLevelChanged() {
            updateHandlePosition()
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onPressed: { zoomslider.active = true; fadeTimer.restart() }
        onReleased: fadeTimer.restart()
        onEntered: { zoomslider.active = true; fadeTimer.restart() }
        onExited: fadeTimer.restart()
        onClicked: { zoomslider.active = true; fadeTimer.restart() }
        onDoubleClicked: { zoomslider.active = true; fadeTimer.restart() }
        onWheel: { zoomslider.active = true; fadeTimer.restart() }
    }

    // Button
    Button {
        id: minusButton
        width: 28
        height: 28
        anchors.left: parent.left
        anchors.leftMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        onClicked: {
            if (!zoomTarget) return
            let newVal = Math.max(zoomMin, zoomTarget.zoomLevel - zoomStep)
            zoomTarget.zoomLevel = newVal
            updateHandlePosition()
            zoomslider.active = true
            fadeTimer.restart()
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

    // Button
    Button {
        id: plusButton
        width: 28
        height: 28
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        onClicked: {
            if (!zoomTarget) return
            let newVal = Math.min(zoomMax, zoomTarget.zoomLevel + zoomStep)
            zoomTarget.zoomLevel = newVal
            updateHandlePosition()
            zoomslider.active = true
            fadeTimer.restart()
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

    // 🔢 Min / Max Labels (optional: hide if you only want buttons)
    /*
    Text {
        ...
    }
    */

    // 🟦 Trapezoid Track
    Canvas {
        id: trapezoid
        anchors.left: minusButton.right
        anchors.right: plusButton.left
        anchors.margins: 8
        height: 40
        anchors.verticalCenter: parent.verticalCenter

        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            const leftHeight = 6
            const rightHeight = 20
            ctx.beginPath()
            ctx.moveTo(0, height / 2 - leftHeight)
            ctx.lineTo(width, height / 2 - rightHeight)
            ctx.lineTo(width, height / 2 + rightHeight)
            ctx.lineTo(0, height / 2 + leftHeight)
            ctx.closePath()
            ctx.fillStyle = isDarkTheme ? "#444" : "#dddddd"
            ctx.fill()
        }
    }

    // Handle (Styled like knob)
    Rectangle {
        id: handle
        width: 36
        height: 22
        radius: 5
        opacity: 0.7
        color: "#222"
        border.color: "#00ffff"
        border.width: 2
        anchors.verticalCenter: trapezoid.verticalCenter

        Component.onCompleted: updateHandlePosition()

        Text {
            anchors.centerIn: parent
            text: zoomTarget ? zoomTarget.zoomLevel.toFixed(1) : "-"
            color: "white"
            font.pixelSize: 11
            font.bold: true
        }

        MouseArea {
            anchors.fill: parent
            anchors.margins: -10
            drag.target: parent
            drag.axis: Drag.XAxis
            drag.minimumX: minusButton.width + 8
            drag.maximumX: zoomslider.width - plusButton.width - 8 - parent.width

            onPressed: {
                zoomslider.active = true
                fadeTimer.restart()
            }

            onPositionChanged: {
                zoomslider.active = true
                fadeTimer.restart()
                const usableWidth = zoomslider.width - 2 * (plusButton.width + 8) - parent.width
                const ratio = (handle.x - minusButton.width - 8) / usableWidth
                const val = zoomMin + ratio * (zoomMax - zoomMin)
                const stepped = Math.round(val / zoomStep) * zoomStep
                if (zoomTarget) zoomTarget.zoomLevel = stepped
            }

            cursorShape: Qt.PointingHandCursor
        }
    }
}
