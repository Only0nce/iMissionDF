import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.0
import QtQuick.Controls.Material 2.15

Item {
    id: root
    visible: true
    width: 1205
    height: 400

    property var pendingDSP: ({})
    property real pendingCenterFreq: 0
    property var pendingUIParams: ({})
    property bool startDSPAfter : false

    // property bool modifyPreset: homeDisplay.modifyPreset
    function editPreset(id,name) {
        modifyPreset = true
        modifyPresetId = id
        modifyPresetName = name
        stackView.pop(null)
        listView.currentIndex = 0
    }

    Dialog {
        id: presetActionDialog
        modal: false
        title: presetActionDialog.presetName
        width: 400
        height: 200
        x: (parent.width/2)-(width/2)
        property string presetId: ""
        property string presetName: ""

        standardButtons: Dialog.Cancel

        contentItem: RowLayout {
            y: 4
            height: 75
            ToolButton {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 35
                Rectangle {
                    color: "#aa009688"
                    radius: 5
                    border.color: "#ffffff"
                    border.width: 0
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    Image {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        anchors.topMargin: 8
                        anchors.bottomMargin: 8
                        source: "images/edit2.png"
                        fillMode: Image.PreserveAspectFit
                    }
                }
                // text: "Modify"
                onClicked: {
                    editPreset(presetActionDialog.presetId,presetActionDialog.presetName)
                    presetActionDialog.close()
                }
            }

            ToolButton {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 35
                Rectangle {
                    color: "#aa009688"
                    radius: 5
                    border.color: "#ffffff"
                    border.width: 0
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    Image {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        anchors.topMargin: 8
                        anchors.bottomMargin: 8
                        source: "images/delete.png"
                        fillMode: Image.PreserveAspectFit
                    }
                }
                // text: "Delete"
                onClicked: {
                    console.log("MemoryAddEdit Delete.... sendupdateWebSlot")
                    configManager.deletePreset(presetActionDialog.presetId)
                    configManager.saveToFile("/var/lib/openwebrx/preset.json")
                    // refresh list
                    let presets = configManager.getPresetsAsList()
                    radioMemList.clear()
                    for (let i = 0; i < presets.length; i++) {
                        radioMemList.append(presets[i])
                    }
                    presetActionDialog.close()
                    mainWindows.deleteCardWebSlot(presetActionDialog.presetId)
                }
            }
        }
    }

    GridView {
        anchors.fill: parent
        anchors.margins: 8
        cellWidth: 132
        cellHeight: 90
        model: radioMemList

        delegate: ToolButton {
            width: 130
            height: 88
            property string profileId: model.profileId
            property string presetName: model.name
            property bool isPermanent: model.isPermanent === true
            contentItem:Rectangle {
                id: bgRect
                radius: 8
                color: isPermanent ? "#10ccccff" : (model.isNew ? "#10ffc0a0" : "#00eeeeee")
                border.color: "#10888888"

                // Animate new profiles
                // SequentialAnimation on color {
                //     running: model.isNew
                //     loops: 1
                //     ColorAnimation { to: "#aaffc0a0"; duration: 120 }
                //     ColorAnimation { to: "#00eeeeee"; duration: 400 }
                // }

                Column {
                    spacing: 4
                    anchors.centerIn: parent

                    Image {
                        source: isPermanent ? "images/newfmradio.png" : "images/fmradio3.png"
                        width: 62
                        height: 48
                        fillMode: Image.PreserveAspectFit
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Label {
                        text: model.name
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }

            onPressAndHold: {
                presetActionDialog.presetId = profileId
                presetActionDialog.presetName = presetName
                presetActionDialog.open()
            }

            onClicked: {
                var setCenterFreq = center_freq
                console.log("setCenterFreq",setCenterFreq, " center_freq:", center_freq);

                var dspcontrolParams = {
                    type: "dspcontrol",
                    params: {
                        "low_cut": low_cut,
                        "high_cut": high_cut,
                        "offset_freq": offset_freq,
                        "mod": mod,
                        "dmr_filter": dmr_filter,
                        "audio_service_id": audio_service_id,
                        "squelch_level": squelch_level,
                        "secondary_mod": secondary_mod
                    }
                }

                if (mainWindows && typeof mainWindows.sendmessage === "function") {
                    mainWindows.sendmessage('{"type":"setfrequency","params":{"frequency":' + setCenterFreq + ',"key":"memagic"}}')

                    // Set up delayed data
                    pendingDSP = dspcontrolParams
                    pendingCenterFreq = setCenterFreq
                    pendingUIParams = dspcontrolParams.params
                    startDSPAfter = true
                    dspDelayTimer.restart()
                } else {
                    console.error("mainWindows or sendmessage() is not available");
                }
            }
        }
    }
    Timer {
        id: dspDelayTimer
        interval: 500 // 1 second
        repeat: false
        onTriggered: {
            // Send delayed dspcontrol message
            mainWindows.sendmessage(JSON.stringify(pendingDSP))

            // Update UI
            radioScanner.spectrumGLPlot.centerFreq = pendingCenterFreq
            radioScanner.spectrumGLPlot.low_cut = pendingUIParams.low_cut
            radioScanner.spectrumGLPlot.high_cut = pendingUIParams.high_cut
            radioScanner.spectrumGLPlot.offsetFrequency = pendingUIParams.offset_freq
            console.log("MemoryAddEdit dspDelayTimer::",pendingUIParams.mod)
            radioScanner.spectrumGLPlot.start_mod = pendingUIParams.mod
            scanSqlLevel = (pendingUIParams.squelch_level * 2) + 255
            stackView.pop(null)
            listView.currentIndex = 0

            // Optionally send "start" command
            if (startDSPAfter) {
                let dspcontrolStart = {
                    type: "dspcontrol",
                    action: "start"
                }
                mainWindows.sendmessage(JSON.stringify(dspcontrolStart))
            }
        }
    }
    // Component.onCompleted: {
    //     mainWindows.refreshProfiles();
    // }
}
