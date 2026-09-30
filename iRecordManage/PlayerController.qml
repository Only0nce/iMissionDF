// PlayerController.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import "../ui"

Item {
    id: playerControllerRoot
    width: 220
    height: 60
    clip: true

    /* ========= Public API ========= */
    property bool isDarkTheme: Material.theme === Material.Dark
    Theme { id: hmiTheme; darkMode: playerControllerRoot.isDarkTheme }
    property bool playing: false
    property int  iconSize: 28
    property int  circleButton: 52
    property int  squareButton: 44
    property int stepMs: 500
    property real scanSqlLevels: 0
    readonly property color transportIconColor: isDarkTheme ? "#FFFFFF" : "#111111"

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
            hoverEnabled: true
            Layout.alignment: Qt.AlignVCenter
            width: squareButton; height: squareButton
            Layout.fillHeight: true
            Layout.fillWidth: true
            background: Rectangle {
                radius: hmiTheme.radiusSm
                color: btnPrev.pressed ? hmiTheme.card : hmiTheme.input
                border.color: btnPrev.hovered ? hmiTheme.accentHover : hmiTheme.lineStrong
            }
            contentItem: Text {
                anchors.centerIn: parent
                text: "◀◀"
                color: playerControllerRoot.transportIconColor
                font.pixelSize: 18
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                renderType: Text.NativeRendering
            }
            onClicked: playerControllerRoot.prevRequested()
        }

        // --- Play / Pause (วงกลม) ---
        ToolButton {
            id: btnPlay
            hoverEnabled: true
            width: circleButton; height: circleButton
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.fillHeight: true
            Layout.fillWidth: true
            background: Rectangle {
                radius: width/2
                color: btnPlay.pressed ? Qt.darker(hmiTheme.accent, 1.15) : hmiTheme.accent
                border.color: hmiTheme.accentHover
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
            hoverEnabled: true
            width: squareButton; height: squareButton
            Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
            Layout.fillHeight: true
            Layout.fillWidth: true
            background: Rectangle {
                radius: hmiTheme.radiusSm
                color: btnNext.pressed ? hmiTheme.card : hmiTheme.input
                border.color: btnNext.hovered ? hmiTheme.accentHover : hmiTheme.lineStrong
            }
            contentItem: Text {
                anchors.centerIn: parent
                text: "▶▶"
                color: playerControllerRoot.transportIconColor
                font.pixelSize: 18
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                renderType: Text.NativeRendering
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
