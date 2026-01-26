import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.0

Item {
    id: item1
    width: 600
    height: 380
    property string buttonColor: "#aa009688"
    property string buttonColorUnselect: "#50009688"

    MouseArea {
        id: mouseArea
        anchors.fill: parent
    }

    RowLayout {
        id: layoutReceiverMode
        visible: true
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 45
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 8
        ListView {
            id: listViewAdMode
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignLeft | Qt.AlignTop
            Layout.preferredHeight: 311
            Layout.preferredWidth: 120
            interactive: false
            spacing: 5
            model: adMode
            delegate: MyToolButton {
                buttonID: index
                bColor: scanAdModeSelected == buttonID ? buttonColor : buttonColorUnselect
                bname: name
                toolButton.onClicked: {
                    scanAdModeSelected = buttonID
                    // console.log("scanAdModeSelected",scanAdModeSelected)
                }
            }
        }

        ListView {
            id: listViewReceiverMode
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignLeft | Qt.AlignTop
            Layout.preferredHeight: 330
            Layout.preferredWidth: 120
            interactive: false
            spacing: 5
            visible: scanAdModeSelected == 0 || scanAdModeSelected == 1
            model: scanAdModeSelected == 0 ? receiverAnalogMode : receiverDigitalMode
            delegate: MyToolButton {
                buttonID: index
                bColor: scanReceiverModeSelected == buttonID ? buttonColor : buttonColorUnselect
                bname: name
                toolButton.onClicked: {
                    scanReceiverModeSelected = buttonID
                    // console.log("scanReceiverModeSelected",scanReceiverModeSelected)
                }
            }
        }

        ListView {
            id: listViewBW
            interactive: false
            spacing: 5
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignLeft | Qt.AlignTop
            Layout.preferredHeight: 330
            Layout.preferredWidth: 120
            model: bwModel
            delegate: MyToolButton {
                buttonID: index
                bColor: scanBwSelected == buttonID ? buttonColor : buttonColorUnselect
                bname: name
                toolButton.onClicked: {
                    scanBwSelected = buttonID
                    // console.log("scanBwSelected",scanBwSelected)
                }
            }
        }

        ListView {
            id: listViewUpdate
            interactive: false
            spacing: 5
            model: updateListModel
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignLeft | Qt.AlignTop
            Layout.preferredWidth: 120
            delegate: MyToolButton
            {
                bname: name
                buttonID: index
                bColor: buttonColor
                toolButton.onClicked:
                {
                    if (buttonID == 1)
                    {                        
                        try
                        {
                            scanBandwidth = listViewBW.model.get(scanBwSelected).name
                        }
                        catch (error)
                        {
                            scanBandwidth = ""
                        }

                        console.log("scanAdModeSelected",scanAdModeSelected,"scanReceiverModeSelected",scanReceiverModeSelected,"scanBwSelected",scanBwSelected)
                        var mod_d = "0"
                        var mod_a = "0"
                        var mod_n = "0"
                        var if_bw = 0
                        var jsonMessage = ""
                        if (scanAdModeSelected == 2){

                        }
                        else if(scanAdModeSelected == 0)
                        {
                            mod_a = "F"
                            mod_n = scanReceiverModeSelected.toFixed(0)
                            if_bw = scanBwSelected.toFixed(0)
                        }
                        else if(scanAdModeSelected == 1)
                        {
                            mod_d = scanReceiverModeSelected.toFixed(0)
                            mod_a = scanReceiverModeSelected.toFixed(0)
                            mod_n = "0"
                            if_bw = 0
                        }

                        // jsonMessage = '{"menuID":"setMod", "mod_dan":"'+ mod_d + mod_a + mod_n +'","IF":'+ if_bw +'}'
                        // qmlCommand(jsonMessage)

                        // {"type":"dspcontrol","params":{"low_cut":-4000,"high_cut":4000,"offset_freq":0,"mod":"nfm","dmr_filter":3,"audio_service_id":0,"squelch_level":-150,"secondary_mod":false}}
                        var dspcontrolParams = {
                            type: "dspcontrol",
                            params: {
                                "low_cut":listViewBW.model.get(scanBwSelected).low_cut,
                                "high_cut":listViewBW.model.get(scanBwSelected).high_cut,
                                "offset_freq":radioScanner.spectrumGLPlot.offsetFrequency,
                                "mod":receiverMode.get(scanReceiverModeSelected).text,
                                "dmr_filter":3,
                                "audio_service_id":0,
                                "squelch_level":(scanSqlLevel-255)/2,
                                "secondary_mod":false
                            }
                        }
                        if (mainWindows && typeof mainWindows.sendmessage === "function")
                        {
                            mainWindows.sendmessage(JSON.stringify(dspcontrolParams));
                            radioScanner.spectrumGLPlot.low_cut = listViewBW.model.get(scanBwSelected).low_cut
                            radioScanner.spectrumGLPlot.high_cut = listViewBW.model.get(scanBwSelected).high_cut
                            radioScanner.spectrumGLPlot.offsetFrequency = radioScanner.spectrumGLPlot.offsetFrequency
                        } else {
                            console.error("mainWindows or sendmessage() is not available");
                        }


                        var dspcontrol = {
                            type: "dspcontrol",
                            action: "start"
                        }
                        if (mainWindows && typeof mainWindows.sendmessage === "function")
                        {
                            mainWindows.sendmessage(JSON.stringify(dspcontrol));
                        } else {
                            console.error("mainWindows or sendmessage() is not available");
                        }

                        currentModIndex = scanReceiverModeSelected;
                        scanBwSelectedBackUp = scanBwSelected;

                    }
                    else
                    {
                        scanReceiverModeSelected = currentModIndex
                        scanAdModeSelected = receiverMode.get(scanReceiverModeSelected).modeID
                        scanBandwidth = listViewBW.model.get(scanBwSelectedBackUp).name
                    }
                    drawer.close()
                }
            }
        }
    }

    Label {
        id: label
        text: qsTr("Option decode mode")
        anchors.top: parent.top
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pointSize: 14
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 10
    }


}

/*##^##
Designer {
    D{i:0;formeditorColor:"#000000";formeditorZoom:1.1}D{i:1}
}
##^##*/
