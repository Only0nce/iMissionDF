import QtQuick 2.15
import QtQuick.Controls 2.15
import QtGraphicalEffects 1.15

Rectangle {
    id: doaSelector
    width: 200
    height: 250
    radius: 8
    color: "#20232a88"
    border.color: "#bdc3c7"
    border.width: 1

    property bool active: false
    opacity: active ? 1.0 : 0.3

    property var visibleArray: [ true, true, true, true, true ]
    property var channelColors: ["#00FF00", "#FF0000", "#0000FF", "#FFA500", "#800080"]

    property var vfoConfig: ({})
    property var krakenTarget
    property var doaLogger      // ⭐ เพิ่มตรงนี้

    signal channelVisibilityChanged(int index, bool visible)

    Timer {
        id: fadeTimer
        interval: 2000
        repeat: false
        onTriggered: doaSelector.active = false
    }

    function fadeIn() {
        active = true
        fadeTimer.restart()
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        z: -1
        onEntered: fadeIn()
        onPressed: fadeIn()
    }

    Connections {
        target: Krakenmapval
        function onSendVfoConfig(config) {
            if (!config || !config.VFOFrequency || config.VFOFrequency.length === 0) {
                let centerFreq = Krakenmapval.centerFrequency !== undefined
                    ? Number(Krakenmapval.centerFrequency).toFixed(3)
                    : "-"
                if (doaLogger) {
                    doaLogger.saveVfoConfig({
                        VFOFrequency: [ centerFreq ]
                    })
                }
                doaSelector.vfoConfig = {
                    VFOFrequency: [ centerFreq ]
                }
                console.log("Center Frequency:", centerFreq)
            } else {
                if (doaLogger) {
                    doaLogger.saveVfoConfig(config)
                }
                doaSelector.vfoConfig = config
                // console.log("DoaSelector received VFO config:", JSON.stringify(config))
            }
        }
    }

    Column {
        width: parent.width - 20
        height: 200
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 20
        anchors.rightMargin: 10
        spacing: 10

        Repeater {
            model: 5
            CheckBox {
                id: chBox
                checked: true
                text: ""
                enabled: doaSelector.active
                opacity: enabled ? 1.0 : 0.3
                indicator: Rectangle {
                    implicitWidth: 20
                    implicitHeight: 20
                    radius: 4
                    color: chBox.checked ? "#169976" : "#222"
                    border.color: "#00ffff"
                    border.width: 2
                }

                contentItem: Row {
                    spacing: 8
                    Text {
                        text: {
                            let freqStr = ""
                            if (doaSelector.vfoConfig &&
                                doaSelector.vfoConfig.VFOFrequency &&
                                doaSelector.vfoConfig.VFOFrequency.length > index) {
                                let freq = Number(doaSelector.vfoConfig.VFOFrequency[index])
                                if (!isNaN(freq)) {
                                    freqStr = freq.toFixed(3) + " MHz"
                                }
                            }
                            return "VFO-" + index + (freqStr ? "  " + freqStr : "")
                        }
                        color: "white"
                        font.pixelSize: 12
                        leftPadding: 20
                    }

                    Rectangle {
                        width: 20
                        height: 12
                        radius: 2
                        color: doaSelector.channelColors[index]
                        border.color: "#888"
                        border.width: 1
                    }
                }

                onCheckedChanged: {
                    doaSelector.fadeIn()
                    doaSelector.visibleArray[index] = checked
                    doaSelector.channelVisibilityChanged(index, checked)
                }
            }
        }
    }
}
