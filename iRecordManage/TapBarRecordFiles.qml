import QtQuick 2.15
import QtQuick.Layouts 1.15

import QtWebSockets 1.0
import QtQuick.Extras 1.4
import QtQuick.VirtualKeyboard 2.15
import QtQuick.VirtualKeyboard.Styles 2.15
import QtQuick.VirtualKeyboard.Settings 2.15
import QtGraphicalEffects 1.0
import QtQuick.Controls.Styles 1.4
import QtQuick.Controls 1.4 as OldControls   // TabView, Tab, TabViewStyle
import QtQuick.Controls 2.15 as Controls     // ToolButton, ScrollBar ฯลฯ

Item {
    id: tapBarRecorderFilesRoot
    visible: true
    width: 1980
    height: 1080
    // เดิมสำหรับไฟล์
    signal waveFilesSelected(var filesArray)
    property bool isDarkTheme: false
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
        color: "#000405"
        anchors.fill: parent
        anchors.topMargin: 60
        // ==== ใช้ Controls 1.4 ด้วย alias OldControls ====
        OldControls.TabView {
            id: tabView
            anchors.fill: parent

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

                    Controls.ScrollBar.vertical: Controls.ScrollBar {
                        policy: Controls.ScrollBar.AlwaysOn
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
                    color: "#e7e6e6"
                }

                Flickable {
                    id: flickableHistory
                    anchors.fill: parent
                    clip: true

                    RegisterDevice {
                        id: registerDevice
                        anchors.fill: parent
                    }

                    Controls.ScrollBar.vertical: Controls.ScrollBar {
                        policy: Controls.ScrollBar.AlwaysOn
                    }
                }

                function secondPageRequest() {
                    var getRegisterDevicePage = '{"menuID":"getRegisterDevicePage"}'
                    qmlCommand(getRegisterDevicePage)
                }
            }

            // =============== STYLE ของ TabView (ยังเป็นของ Controls 1.4) ===============
            // =============== STYLE ของ TabView (ยังเป็น Controls 1.4) ===============
            style: TabViewStyle {
                tab: Rectangle {
                    implicitWidth: 200
                    implicitHeight: 65
                    color: styleData.selected ? "white" : "#d3d3d3"
                    border.color: "black"
                    radius: 5
                    Text {
                        text: styleData.title
                        anchors.centerIn: parent
                        font.pixelSize: 20
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: styleData.selected ? "black" : "#555"
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
