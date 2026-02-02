// PlayerController.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: playerControllerRoot
    width: 220
    height: 60
    clip: true

    /* ========= Public API ========= */
    property bool isDarkTheme: false
    property bool playing: false
    property int  iconSize: 28
    property int  circleButton: 52
    property int  squareButton: 44
    property int stepMs: 500
    property real scanSqlLevels: 0

    signal prevRequested()
    signal nextRequested()
    signal togglePlayRequested(bool wantPlay)

    /* ========= Icon Resolver ========= */
    function iconSrc(name) {
        var map = {
            play:       isDarkTheme ? "qrc:/iRecordManage/images/playLight.png"     : "qrc:/iRecordManage/images/playDark.png",
            pause:      isDarkTheme ? "qrc:/iRecordManage/images/puaseLight.png"    : "qrc:/iRecordManage/images/puaseDark.png",
            skipLeft:   isDarkTheme ? "qrc:/iRecordManage/images/skipLeftLight.png" : "qrc:/iRecordManage/images/skipLeftDark.png",
            skipRight:  isDarkTheme ? "qrc:/iRecordManage/images/skipRighLight.png" : "qrc:/iRecordManage/images/skipRighDark.png"
        }
        return map[name] || ""
    }

    /* ========= Layout ========= */
    RowLayout {
        id: bar
        anchors.fill: parent
        anchors.margins: 8
        spacing: 12

        // --- Skip Left ---
        ToolButton {
            id: btnPrev
            Layout.alignment: Qt.AlignVCenter
            width: squareButton; height: squareButton
            Layout.fillHeight: true
            Layout.fillWidth: true
            background: Rectangle {
                radius: 10
                color: playerControllerRoot.isDarkTheme ?  "#e9eef5" : "#2a2f37"
                border.color: playerControllerRoot.isDarkTheme ?  "#cad3df" : "#353b45"
            }
            contentItem: Image {
                anchors.centerIn: parent
                width: iconSize; height: iconSize
                fillMode: Image.PreserveAspectFit
                source: iconSrc("skipLeft")
                onStatusChanged: if (status === Image.Error) console.warn("icon error:", source)
            }
            onClicked: playerControllerRoot.prevRequested()
        }

        // --- Play / Pause (วงกลม) ---
        ToolButton {
            id: btnPlay
            width: circleButton; height: circleButton
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.fillHeight: true
            Layout.fillWidth: true
            background: Rectangle {
                radius: width/2
                color: playerControllerRoot.isDarkTheme ?  "#e9eef5" : "#2a2f37"
                border.color: playerControllerRoot.isDarkTheme ?  "#cad3df" : "#353b45"
            }
            contentItem: Image {
                id: playIcon
                anchors.centerIn: parent
                width: iconSize + 2; height: iconSize + 2
                fillMode: Image.PreserveAspectFit
                source: playerControllerRoot.playing ? iconSrc("pause") : iconSrc("play")
                onStatusChanged: if (status === Image.Error) console.warn("icon error:", source)
            }
            onClicked: {
                var wantPlay = !playerControllerRoot.playing
                // if(wantPlay === true){
                    // console.log("wantPlay:",wantPlay," wsClient.setSpeakerVolumeMute(1)")
                    // wsClient.setSpeakerVolumeMute(1)
                    // mainWindows.setSqlOffManual();
                // }
                // else{
                    // wsClient.setSpeakerVolumeMute(0)
                    // console.log("wantPlay:",wantPlay," wsClient.setSpeakerVolumeMute(0)")
                // }
                // wsClient.setSpeakerVolumeMute(1)
                // mainWindows.setSqlOffManual();
                playerControllerRoot.togglePlayRequested(wantPlay)


            }
        }

        // --- Skip Right ---
        ToolButton {
            id: btnNext
            width: squareButton; height: squareButton
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            Layout.fillHeight: true
            Layout.fillWidth: true
            background: Rectangle {
                radius: 10
                color: playerControllerRoot.isDarkTheme ?  "#e9eef5" : "#2a2f37"
                border.color: playerControllerRoot.isDarkTheme ?  "#cad3df" : "#353b45"
            }
            contentItem: Image {
                anchors.centerIn: parent
                width: iconSize; height: iconSize
                fillMode: Image.PreserveAspectFit
                source: iconSrc("skipRight")
                onStatusChanged: if (status === Image.Error) console.warn("icon error:", source)
            }
            onClicked: playerControllerRoot.nextRequested()
        }

    }
}

/*##^##
Designer {
    D{i:0;formeditorZoom:3}
}
##^##*/
