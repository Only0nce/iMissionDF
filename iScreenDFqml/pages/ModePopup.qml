import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15

Item {
    id: modePopup
    anchors.fill: parent
    visible: false
    z: 9999
    focus: true

    // ==== PROPERTIES ====
    property string remoteStatus: "LOCAL"
    property int waitSeconds: 20
    property int secondsLeft: 0
    property bool countdownCanceled: false

    // ==== PUBLIC API ====
    function open() {
        visible = true
        countdownCanceled = false

        if (remoteStatus === "" || remoteStatus === "LOCAL") {
            remoteStatus = "LOCAL"
            secondsLeft = waitSeconds
            autoTimer.start()
        } else {
            secondsLeft = 0
            autoTimer.stop()
        }
    }

    function close() {
        autoTimer.stop()
        visible = false
    }

    // ===== DIM BACKGROUND =====
    Rectangle {
        anchors.fill: parent
        color: "#00000088"

        MouseArea {
            anchors.fill: parent
            onClicked: {
                countdownCanceled = true
                autoTimer.stop()
                Krakenmapval.setMode("LOCAL")
                modePopup.close()
            }
        }
    }

    // ===== POPUP CARD =====
    Rectangle {
        id: card
        width: 400
        height: 240
        radius: 16
        anchors.centerIn: parent
        color: "#1F2224"
        border.color: "#4CE4C9"
        border.width: 1.2

        layer.enabled: true
        layer.effect: DropShadow {
            horizontalOffset: 0
            verticalOffset: 4
            radius: 18
            samples: 32
            color: "#1FBBFF55"
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 22
            spacing: 14

            // ----- Title -----
            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Image {
                    source: "qrc:/icons/wifi-lock.png"
                    width: 28; height: 28
                }

                Label {
                    text: "Remote Mode Control"
                    font.pixelSize: 22
                    font.bold: true
                    color: "#7AE2CF"
                }
            }

            // ----- Description -----
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 17
                color: "#D3D3D3"

                text: {
                    if (remoteStatus === "LOCAL") {
                        if (countdownCanceled)
                            return "Current Mode: LOCAL\nThe countdown has been canceled."
                        return "Another device is requesting Remote Mode.\n" +
                               "The system will switch to REMOTE automatically in " +
                               secondsLeft + " seconds."
                    }
                    return "Current Mode: REMOTE\nThe system is now ready for remote access."
                }
            }

            Item { Layout.fillHeight: true }

            // ----- BUTTONS -----
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Button {
                    text: "Cancel"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42

                    onClicked: {
                        countdownCanceled = true
                        Krakenmapval.setMode("LOCAL")
                        autoTimer.stop()
                        modePopup.close()
                    }
                }

                Button {
                    text: "OK"
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42

                    onClicked: {
                        countdownCanceled = true
                        autoTimer.stop()
                        if (remoteStatus === "LOCAL")
                            Krakenmapval.setMode("REMOTE")
                        modePopup.close()
                    }
                }
            }
        }
    }

    // ==== AUTO TIMER ====
    Timer {
        id: autoTimer
        interval: 1000
        repeat: true
        running: false

        onTriggered: {
            if (secondsLeft > 0)
                secondsLeft--

            if (secondsLeft <= 0) {
                stop()
                if (!countdownCanceled && remoteStatus === "LOCAL")
                    Krakenmapval.setMode("REMOTE")
            }
        }
    }

    // ==== SIGNAL FROM C++ ====
    Connections {
        target: Krakenmapval
        onUpdateParameterMode: {
            modePopup.remoteStatus = mode
            if (!modePopup.visible) return

            if (mode === "LOCAL" && !countdownCanceled) {
                secondsLeft = waitSeconds
                autoTimer.start()
            } else if (mode === "REMOTE") {
                autoTimer.stop()
                secondsLeft = 0
            }
        }
    }
}
