import QtQuick 2.12

Item {
    id: root_drawerItem
    width: parent.width
    height: parent.height
    property int itemShow: 0
    //property real itemWidth: itemShow == 1 ? volumeCtrl.width : itemShow == 2 ? sqlCtrl.width  : itemShow == 3 ? toolLevel.width : sCanRf.width ? itemShow == 4 : itemShow == 5 ? audioCtrl.width : 100
    property real itemWidth: itemShow == 1 ? volumeCtrl.width : itemShow == 2 ? sqlCtrl.width  : itemShow == 3 ? toolLevel.width : itemShow == 4 ? sCanRf.width : itemShow == 5 ? audioCtrl.width : 100

    property alias drawerItem: drawerItem
    property alias volumeCtrl: volumeCtrl
    property alias audioCtrl: audioCtrl
    property alias sqlCtrl: sqlCtrl
    property alias toolLevel: toolLevel
    property alias sCanRf: sCanRf
    property bool opened: false

    // // ในไฟล์ที่มี SQLDrawer นี้
    // property int scanSqlLevel: 0
    // property int scanSqlMode: 0


    function close() {
        opened = false;
        drawerAnim.to = width;
        drawerAnim.start();
        closeDrawerTimer.stop();
    }

    function open(which) {
        // dlog("open() with which =", which)
        itemShow = which
        opened = true;
        drawerAnim.to = width - (drawerItem.width+10);
        drawerAnim.start();
        closeDrawerTimer.stop();
        // console.log("open",which)

        if(itemShow == 3){
            // console.log("itemShow == 3::",itemShow)
            closeDrawerTimer.stop()
        }
        else if(itemShow == 4){
            // console.log("itemShow == 4::",itemShow)
            closeDrawerTimer.stop()
        }
        else{
            console.log("else itemShow::",itemShow)
            closeDrawerTimer.start()
        }
    }


    Rectangle {
        anchors.fill: parent
        visible: root_drawerItem.opened
        color: "#50303030"
        MouseArea {
            anchors.fill: parent
            onClicked: root_drawerItem.close()
        }
    }
    Rectangle {
        id: drawerItem
        width: itemWidth
        height: 380
        x: root_drawerItem.width
        color: "#e6000000"
        radius: 10
        border.width: 0
        anchors.verticalCenter: parent.verticalCenter
        visible: true
        z: 99

        // ===== API ภายนอก =====

        // function open() {
        //     opened = true;
        //     drawerAnim.to = root_drawerItem.width - (width+10);
        //     drawerAnim.start();
        //     closeDrawerTimer.stop();

        //     if(itemShow == 3){
        //         console.log("itemShow == 3::",itemShow)
        //         closeDrawerTimer.stop()
        //     }
        //     else if(itemShow == 4){
        //         console.log("itemShow == 4::",itemShow)
        //         closeDrawerTimer.stop()
        //     }
        //     else{
        //         console.log("else itemShow::",itemShow)
        //         closeDrawerTimer.start()
        //     }
        // }


        NumberAnimation on x
        {
            id: drawerAnim
            duration: 100
        }

        Timer {
            id:closeDrawerTimer
            interval: 5000
            running: false
            repeat: false
            onTriggered: root_drawerItem.close()
        }

        VolDrawer
        {
            id: audioCtrl
            visible: itemShow == 5
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            width: 240
            height: drawerItem.height
            ctrlLevel: scanVolLevel
            audioLevel: scanAudioLevel
            headphoneCtrlLevel: scanVolLevelHeadphone
            stringVolume: ((scanVolLevel-255)/2).toFixed(1) +" dB"
            stringAudio: scanMuteOn ? "Mute" : scanAudioLevel +" %"
            stringHead: ((scanVolLevelHeadphone-255)/2).toFixed(1) +" dB"
            mute: scanMuteOn
            volumeHeadphoneCtrlLevel.slider.onValueChanged: {
                console.log("audioCtrl>>",scanVolLevelHeadphone)
                scanVolLevelHeadphone = volumeHeadphoneCtrlLevel.slider.value
                closeDrawerTimer.restart()
            }
            audioCtrlLevel.slider2.onValueChanged: {
                console.log("audioCtrl>>",scanAudioLevel)
                scanAudioLevel = audioCtrlLevel.slider2.value
                closeDrawerTimer.restart()
            }
            volumeCtrlLevel.slider.onValueChanged: {
                console.log("audioCtrl>>",scanVolLevel)
                scanVolLevel = volumeCtrlLevel.slider.value
                closeDrawerTimer.restart()
            }
            onMuteChanged:
            {
                scanMuteOn = mute
            }
        }


        VolumeDrawer
        {
            id: volumeCtrl
            visible: itemShow == 1
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            width: 240
            height: drawerItem.height
            ctrlLevel: scanVolLevel
            audioLevel: scanAudioLevel
            headphoneCtrlLevel: scanVolLevelHeadphone
            stringVolume: ((scanVolLevel-255)/2).toFixed(1) +" dB"
            stringAudio: scanMuteOn ? "Mute" : scanAudioLevel +" %"
            stringHead: ((scanVolLevelHeadphone-255)/2).toFixed(1) +" dB"
            mute: scanMuteOn
            volumeHeadphoneCtrlLevel.slider.onValueChanged: {
                console.log("audioCtrl>>",scanVolLevelHeadphone)
                scanVolLevelHeadphone = volumeHeadphoneCtrlLevel.slider.value
                closeDrawerTimer.restart()
            }
            audioCtrlLevel.slider2.onValueChanged: {
                console.log("audioCtrl>>",scanAudioLevel)
                scanAudioLevel = audioCtrlLevel.slider2.value
                closeDrawerTimer.restart()
            }
            volumeCtrlLevel.slider.onValueChanged: {
                console.log("audioCtrl>>",scanVolLevel)
                scanVolLevel = volumeCtrlLevel.slider.value
                closeDrawerTimer.restart()
            }
            onMuteChanged:
            {
                scanMuteOn = mute
            }
        }
        SQLDrawer
        {
            id: sqlCtrl
            visible: itemShow == 2
            width: 120
            height: drawerItem.height
            ctrlLevel: scanSqlLevel
            currentSqlType: scanSqlMode
            volumeCtrlLevel.slider.onValueChanged: {
                // console.log("volumeCtrlLevel.slider.value",volumeCtrlLevel.slider.value)
                scanSqlLevel = volumeCtrlLevel.slider.value || 0
                closeDrawerTimer.restart()
            }
            // onCurrentSqlTypeChanged: {
            //     scanSqlMode = currentSqlType
            //     closeDrawerTimer.restart()
            // }
            // onCtrlLevelChanged: {
            //     closeDrawerTimer.restart()
            // }
        }

        ReceiveModeScanner
        {
            id: toolLevel
            width: 600
            visible: itemShow == 3
            height: drawerItem.height
            onVisibleChanged: {
                closeDrawerTimer.stop()
            }
        }

        FindBandsWithProfile
        {
            id: sCanRf
            width: 600
            visible: itemShow == 4
            height: drawerItem.height
            onVisibleChanged: {
                closeDrawerTimer.stop()
            }
        }
    }
}
