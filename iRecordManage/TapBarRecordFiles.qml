//TapBarRecordFiles.qml
import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15

import QtWebSockets 1.0
import QtQuick.Extras 1.4
import QtQuick.VirtualKeyboard 2.15
import QtQuick.VirtualKeyboard.Styles 2.15
import QtQuick.VirtualKeyboard.Settings 2.15
import QtGraphicalEffects 1.0
import QtQuick.Controls.Styles 1.4
import QtQuick.Controls 1.4 as OldControls   // TabView, Tab, TabViewStyle
import QtQuick.Controls 2.15 as Controls     // ToolButton, ScrollBar ฯลฯ
import "../ui"

Item {
    id: tapBarRecorderFilesRoot
    visible: true
    width: parent ? parent.width : 1980
    height: parent ? parent.height : 1080
    // เดิมสำหรับไฟล์
    signal waveFilesSelected(var filesArray)
    property bool isDarkTheme: Material.theme === Material.Dark
    Theme { id: hmiTheme; darkMode: tapBarRecorderFilesRoot.isDarkTheme }
    property bool muted: false
    property int  circleButton: 60
    property int  squareButton: 60
    property real  beforemuteAudio: 0
    property int  iconSize: 55
    signal sendMessage(string msg)
    property var mainRoot
    signal wavePlayToggleRequested(bool wantPlay, var filesArray, bool concatMode, int playPosMs)

    function iconSrc(name) {
        console.log("iconSrc", name)
        var map = {
            mute:    isDarkTheme ? "qrc:/iRecordManage/images/muteVolumeLight.png"
                                 : "qrc:/iRecordManage/images/muteVolumeDark.png",
            unmute:  isDarkTheme ? "qrc:/iRecordManage/images/unmuteVolumeLight.png"
                                 : "qrc:/iRecordManage/images/unmuteVolumeDark.png",

        }
        return map[name] || ""
    }

    Rectangle {
        id: rectangle
        color: hmiTheme.page
        anchors.fill: parent
        anchors.topMargin: 60
        // ==== ใช้ Controls 1.4 ด้วย alias OldControls ====
        OldControls.TabView {
            id: tabView
            anchors.fill: parent
            anchors.margins: 12

            // =============== TAB 0: RECORDING FILES ===============
            OldControls.Tab {
                id: tabRecording
                title: "RECORDING FILES"
                width: 20
                height: 10

                Flickable {
                    id: flickable
                    anchors.fill: parent
                    clip: true

                    RecordFiles {
                        id: recordFiles
                        anchors.fill: parent
                        isDarkTheme: tapBarRecorderFilesRoot.isDarkTheme
                        onWaveFilesSelected: tapBarRecorderFilesRoot.waveFilesSelected(filesArray)

                        onWavePlayToggleRequested: {
//                            console.log("[TapBarRecordFiles] wavePlayToggleRequested from RecordFiles")
                            tapBarRecorderFilesRoot.wavePlayToggleRequested(
                                        wantPlay,
                                        filesArray,
                                        concatMode,
                                        playPosMs)
                        }
                    }

                    // Outer workspace flickables fit on screen; the persistent
                    // page-level scrollbar is unused and should stay hidden in
                    // both dark and light themes.
                    Controls.ScrollBar.vertical: Controls.ScrollBar {
                        policy: Controls.ScrollBar.AlwaysOff
                    }
                }

                function firstPageRequest() {
                    var getRecordFiles = '{"menuID":"getRecordFiles"}'
                    qmlCommand(getRecordFiles)
                }
            }

            // =============== TAB 1: REGISTER DEVICES ===============
            OldControls.Tab {
                id: tabRegister
                title: "REGISTER DEVICES"

                Rectangle {
                    anchors.fill: parent
                    color: hmiTheme.page
                }

                Flickable {
                    id: flickableHistory
                    anchors.fill: parent
                    clip: true

                    RegisterDevice {
                        id: registerDevice
                        anchors.fill: parent
                        isDarkTheme: tapBarRecorderFilesRoot.isDarkTheme
                    }

                    // Outer workspace flickables fit on screen; the persistent
                    // page-level scrollbar is unused and should stay hidden in
                    // both dark and light themes.
                    Controls.ScrollBar.vertical: Controls.ScrollBar {
                        policy: Controls.ScrollBar.AlwaysOff
                    }
                }

                function secondPageRequest() {
                    var getRegisterDevicePage = '{"menuID":"getRegisterDevicePage"}'
                    qmlCommand(getRegisterDevicePage)
                }
            }
            // =============== TAB 2: MONITOR DISPLAY ===============
//            OldControls.Tab {
//                id: tabMonitor
//                title: "MONITOR DISPLAY"

//                Rectangle {
//                    anchors.fill: parent
//                    color: "#234654"
//                }
//                MonitorDisplay {
//                    id: rootmonitorView
//                    anchors.fill: parent

//                    cpuPct: monitorCpuPct
//                    updatedText: monitorTs

////                    // RAM
//                    memUsedMB: monitorMemUsed
//                    memTotalMB: monitorMemTotal
//                    percentRAMUsed: monitorPercentRAM

////                    // STORAGE
//                    storageUsedGB: monitorStorageUsed
//                    storageTotalGB: monitorStorageTotal

//                    // ✅ TOP (uptime/load/tasks)
//                    hTopUptimeText: monitorUptimeText
//                    hTopLoad1: monitorLoad1
//                    hTopLoad5: monitorLoad5
//                    hTopLoad15: monitorLoad15
//                    hTopTasksTotal: monitorTasksTotal
//                    hTopThreadsTotal: monitorThreadsTotal
//                    hTopTasksRunning: monitorTasksRunning
//                }

//            }

            // =============== STYLE ของ TabView (ยังเป็น Controls 1.4) ===============
            style: TabViewStyle {
                frameOverlap: 0
                tabOverlap: 0
                frame: Rectangle {
                    color: hmiTheme.panel
                    border.width: 1
                    border.color: hmiTheme.line
                    radius: hmiTheme.radiusLg
                }
                tab: Rectangle {
                    implicitWidth: 220
                    implicitHeight: 52
                    color: styleData.selected ? hmiTheme.accent : hmiTheme.cardAlt
                    border.color: styleData.selected ? hmiTheme.accent : hmiTheme.lineStrong
                    border.width: 1
                    radius: hmiTheme.radiusSm
                    Text {
                        text: styleData.title
                        anchors.centerIn: parent
                        font.pixelSize: 14
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: styleData.selected ? "#061514" : hmiTheme.text
                    }
                }
            }


            // =============== ยิงคำสั่งเมื่อสลับแท็บ ===============
            onCurrentIndexChanged: {
                if (currentIndex === 0) {
                    tabRecording.firstPageRequest()
                } else if (currentIndex === 1) {
                    tabRegister.secondPageRequest()
                }
            }

            Component.onCompleted: {
                if (currentIndex === 0) {
                    tabRecording.firstPageRequest()
                } else if (currentIndex === 1) {
                    tabRegister.secondPageRequest()
                }
            }
        }
    }

    // ==== ปุ่ม ToolButton ใช้ Controls 2.15 ====
    Controls.ToolButton {
        id: btnMute
//        x: 1850
//        y: 60          // ✅ ดันลงมา

//        width: squareButton
//        height: squareButton

//        background: Rectangle {
//            radius: 10
//            color: isDarkTheme ? "#e9eef5" : "#2a2f37"
//            border.color: isDarkTheme ? "#cad3df" : "#353b45"
//        }

//        contentItem: Image {
//            anchors.centerIn: parent
//            width: iconSize
//            height: iconSize
//            fillMode: Image.PreserveAspectFit
//            source: muted ? iconSrc("mute") : iconSrc("unmute")
//            onStatusChanged: if (status === Image.Error)
//                                 console.warn("icon error:", source)
//        }

//        onClicked: {

//            var payload = {
//                menuID: muted ? "muteVolume" : "unmuteVolume",
//                muted: muted
//            }
//            console.log("status mute is:", muted, " then beforemuteAudio:",beforemuteAudio)
//            if(!muted){
//                // beforemuteAudio = scanVolLevel
//                mainWindows.setSpeakerVolumeMute(1)
//            }
//            else{
//                mainWindows.setSpeakerVolumeMute(0)
//            }

//            // var msg = JSON.stringify(payload)
//            // console.log("[TapBarRecordFiles] volume clicked:", msg)
//            // if (sendMessageMain) sendMessageMain(msg)
//            // else console.warn("[TapBarRecordFiles] mainRoot or sendMessageMain not available")
//            muted = !muted
//        }
    }


}
