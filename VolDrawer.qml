import QtQuick 2.0
import QtQuick.Controls 2.1
import QtQuick.Layouts 1.0
Item {
    id: item1
    property real ctrlLevel: 0
    property real audioLevel: 0
    property real headphoneCtrlLevel: 0
    property real scanSqlLevels: scanSqlLevel
    property string imagesource: "images/speaker.png"
    property string str_type: "Backlight"
    property string str_level: "0"
    property string str_level2: "0"
    property alias volumeCtrlLevel: volumeCtrlLevel
    property alias audioCtrlLevel: audioCtrlLevel
    property alias volumeHeadphoneCtrlLevel: volumeHeadphoneCtrlLevel
    property real rotate: 270
    property bool mute: true
    property string stringVolume: text3.text
    property string stringHead: text4.text
    property string stringAudio: text9.text

    width: 180
    height: 380

    Component.onCompleted: {
        console.log("VolDrawer Component.onCompleted",mute)
        // rotate = screenrotation
    }

    // onMuteChanged: {
    //     // console.log("mute changed:", mute)
    //     // ทำอะไรเพิ่ม เช่นเรียก wsClient
    // }

    // onCtrlLevelChanged: {
    //     // scanVolLevel = volumeCtrlLevel.inivalue
    //     console.log("wsClient.isMute():", wsClient.isMute(),mute)
    //     // volumeCtrlLevel.inivalue = ctrlLevel
    //     // text3.text = ((ctrlLevel-255)/2).toFixed(1) +" dB"
    //     if(wsClient.isMute() !== mute) mute = wsClient.isMute()
    //     console.log("after wsClient.isMute():", wsClient.isMute(),mute)
    // }

    // onAudioLevelChanged: {
    //     // scanAudioLevel = audioCtrlLevel.inivalue
    //     // console.log("audioLevel changed:", audioLevel,scanAudioLevel)
    //     // audioCtrlLevel.inivalue = audioLevel
    //     // text9.text = mute ? "Mute" : audioLevel +" %"
    //     console.log("wsClient.isMute():", wsClient.isMute(),mute)
    //     if(wsClient.isMute() !== mute) mute = wsClient.isMute()
    //     console.log("after wsClient.isMute():", wsClient.isMute(),mute)
    // }

    // onHeadphoneCtrlLevelChanged: {
    //     // scanVolLevelHeadphone = volumeHeadphoneCtrlLevel.inivalue
    //     // volumeHeadphoneCtrlLevel.inivalue = headphoneCtrlLevel
    //     // text4.text = ((headphoneCtrlLevel-255)/2).toFixed(1) +" dB"
    //     // console.log("headphoneCtrlLevel changed:", headphoneCtrlLevel,scanVolLevelHeadphone)
    //     console.log("wsClient.isMute():", wsClient.isMute(),mute)
    //     if(wsClient.isMute() !== mute) mute = wsClient.isMute()
    //     console.log("after wsClient.isMute():", wsClient.isMute(),mute)
    // }

    // onScanSqlLevelsChanged: {
    //     // console.log("scanSqlLevels changed:", scanSqlLevels)
    //     console.log("wsClient.isMute():", wsClient.isMute(),mute)
    //     if(wsClient.isMute() !== mute) mute = wsClient.isMute()
    //     console.log("after wsClient.isMute():", wsClient.isMute(),mute)
    // }

    Connections {
        target: wsClient
        function onMutedChanged(m) {
            mute = m
            console.log("mutedChanged ->", m)
        }
    }

    Rectangle {
        id: rectangle9
        width: 45
        color: "#00000000"
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 20
        anchors.bottomMargin: 20
        // anchors.rightMargin: 20
        anchors.rightMargin: 140

        Rectangle {
            id: rectangle8
            color: "#33116273"
            radius: 5
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 60
            anchors.bottomMargin: 4
            anchors.left: parent.left
            anchors.leftMargin: 0
            anchors.right: parent.right
            anchors.rightMargin: 0


            AudioVolume {
                id: audioCtrlLevel
                x: 0
                y: 62
                anchors.right: parent.right
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                inivalue: audioLevel
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.preferredWidth: 64
                Layout.preferredHeight: height-20
                levelmax: 100
                levelmin: 0
                slider2.value:inivalue
                progressBar2.value: slider2.value
                progressBar2.onValueChanged:
                {
                    mute = false
                }
            }
            Text {
                id: text9
                x: -13
                y: 24
                width: 71
                height: 45
                color: "#ffffff"
                text: stringAudio
                // text: mute ? "Mute" : audioLevel +" %"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                rotation: rotate
            }
        }

        ToolButton {
            id: iConAudio9
            enabled: true
            x: 0
            width: 45
            height: 45
            anchors.top: parent.top
            Layout.preferredHeight: 200
            Layout.preferredWidth: 200
            contentItem: Image {
                visible: true
                fillMode: Image.Stretch
                horizontalAlignment: Image.AlignHCenter
                source: mute ? "images/speaker_mute.png" : "images/speaker.png"
                // rotation: rotate
                verticalAlignment: Image.AlignVCenter
            }
            onClicked: {
                console.log("before VolDrawer onClicked Mute software",mute,wsClient.isMute())
                mute = !mute
                console.log("after VolDrawer onClicked Mute software",mute,wsClient.isMute())
                if(!mute){
                    // beforemuteAudio = scanVolLevel
                    wsClient.setSpeakerVolumeMute(0)
                    mainWindows.setSqlLevel(0)
                    mainWindows.sendmessage('{"type": "dspcontrol","params": {"squelch_level": '+((0-255)/2).toFixed(1)+'}}')
                    currentSqlLevel = (0-255)/2
                    mainWindows.setSqlOffManual();
                    console.log("currentSqlLevel = (0-255)/2")
                }
                else{
                    wsClient.setSpeakerVolumeMute(1)
                    mainWindows.setSqlLevel(scanSqlLevel)
                    mainWindows.sendmessage('{"type": "dspcontrol","params": {"squelch_level": '+((scanSqlLevel-255)/2).toFixed(1)+'}}')
                    currentSqlLevel = (scanSqlLevel-255)/2
                    mainWindows.setSqlOffManual();
                    console.log("currentSqlLevel = (255-255)/2")
                }
                // mute = !mute
            }
        }
    }

    Rectangle {
        id: rectangle1
        width: 45
        color: "#00000000"
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 20
        anchors.bottomMargin: 20
        anchors.rightMargin: 20
        // anchors.rightMargin: 80

        Rectangle {
            id: rectangle
            color: "#33116273"
            radius: 5
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 60
            anchors.bottomMargin: 4
            anchors.left: parent.left
            anchors.leftMargin: 0
            anchors.right: parent.right
            anchors.rightMargin: 0


            Volume {
                id: volumeCtrlLevel
                x: 0
                y: 62
                anchors.right: parent.right
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                inivalue: ctrlLevel
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.preferredWidth: 64
                Layout.preferredHeight: height-20
                levelmax: 255
                levelmin: 150
                slider.value:inivalue
                progressBar.value: slider.value
                progressBar.onValueChanged:
                {
                    // mute = false
                }
            }
            Text {
                id: text3
                x: -13
                y: 24
                width: 71
                height: 45
                color: "#ffffff"
                text: stringVolume
                // text: ((ctrlLevel-255)/2).toFixed(1) +" dB"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                rotation: rotate
            }
        }

        ToolButton {
            id: iConAudio
            enabled: false
            x: 0
            width: 45
            height: 45
            anchors.top: parent.top
            Layout.preferredHeight: 200
            Layout.preferredWidth: 200
            contentItem: Image {
                visible: true
                fillMode: Image.Stretch
                horizontalAlignment: Image.AlignHCenter
                source: "images/speaker.png"
                // rotation: rotate
                verticalAlignment: Image.AlignVCenter
            }
            // onClicked: mute = !mute
        }
    }

    Rectangle {
        id: headphone
        width: 45
        color: "#00000000"
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        // anchors.rightMargin: 140
        anchors.rightMargin: 80
        anchors.topMargin: 20
        anchors.bottomMargin: 20
        Rectangle {
            id: rectangle4
            color: "#33116273"
            radius: 5
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: 0
            anchors.rightMargin: 0
            anchors.topMargin: 60
            anchors.bottomMargin: 4

            Volume {
                id: volumeHeadphoneCtrlLevel
                x: 0
                y: 62
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                slider.value: inivalue
                progressBar.value: slider.value
                progressBar.onValueChanged: {
                    mute = false
                }
                levelmin: 50
                levelmax: 255
                inivalue: headphoneCtrlLevel
                Layout.preferredWidth: 64
                Layout.preferredHeight: height-20
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
            Text {
                id: text4
                x: -13
                y: 24
                width: 71
                height: 45
                color: "#ffffff"
                text: stringHead
                // text: ((headphoneCtrlLevel-255)/2).toFixed(1) +" dB"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                rotation: rotate
            }
        }

        ToolButton {
            id: iConAudio1
            x: 0
            width: 45
            height: 45
            anchors.top: parent.top
            onClicked: { mute = !mute }
            enabled: false
            contentItem: Image {
                visible: true
                horizontalAlignment: Image.AlignHCenter
                verticalAlignment: Image.AlignVCenter
                source: "images/headphone_spk.png"
                // rotation: rotate
                fillMode: Image.Stretch
            }
            Layout.preferredWidth: 200
            Layout.preferredHeight: 200
        }
    }

    Rectangle {
        id: rectangle2
        x: 0
        width: 30
        height: 590
        color: "#00ffffff"
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 200

        Text {
            id: text1
            color: "#ffffff"
            text: "Volume Control"
            anchors.fill: parent
            font.pixelSize: 16
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            styleColor: "#ffffff"
            rotation: rotate
        }
    }

}



/*##^##
Designer {
    D{i:0;formeditorZoom:1.5}
}
##^##*/
