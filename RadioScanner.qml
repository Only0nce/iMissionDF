import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.0
import QtQuick.Controls.Material 2.4
Item {
    id: scanpage
    // width: 1195
    // height: 400
    property real freqMin: 30e6
    property real freqMax: 3.8e9
    property real freqScan: freqScan
    property bool keyfreqEdit: false
    property string buttonColor: "#aa009688"
    property string buttonColorRotary: "#ee009688"
    property string freqEditColor: "#aaaaaa"
    property real frequencyUnitValueButton: frequencyUnitValue
    property bool componentNotCompleted: false

    property string freqScanUnit: freqUnit

    property real size: notificationList.count
    property alias drawerVolume: drawer
    property alias drawerAudio: drawer
    property alias drawerSql: drawer
    property alias spectrumGLPlot: spectrumGLPlot
    property real cpuDatatemperature: 0.0

    onSizeChanged: {
        homeGridView.currentIndex = notificationList.count-1
    }


    onFrequencyUnitValueButtonChanged:
    {
        if(frequencyUnitValueButton != -1)
        {
            freqUnit = freqUnitList.get(frequencyUnitValueButton).name
        }
    }

    onFreqScanUnitChanged: {
        if (componentNotCompleted == false) return
        if (freqEdit.text != "")
        {
            switch (freqUnit)
            {
            case "Hz":
                freqScan = parseInt(freqEdit.text)
                break
            case "kHz":
                freqScan = parseFloat(freqEdit.text) * 1e3
                break
            case "MHz":
                freqScan = parseFloat(freqEdit.text) * 1e6
                break
            case "GHz":
                freqScan = parseFloat(freqEdit.text) * 1e9
                break
            }
            console.log("onFreqScanUnitChanged frequency",freqScan)
            if ((freqScan >= freqMin) & (freqScan <= freqMax))
            {
                freqEditColor = "#aaaaaa"
            }
            else
            {
                freqEditColor = "#ff5555"
                console.log("frequency out of range")
            }
        }
    }

    Component.onCompleted:
    {
        var str = freqScan.toString()
        switch(freqUnit)
        {
        case "Hz":
            freqScanString = freqScan.toString()
            break;
        case "kHz":
            freqScanString = (freqScan/1e3).toFixed(3)
            break;
        case "MHz":
            freqScanString = (freqScan/1e6).toFixed(4)
            break;
        case "GHz":
            freqScanString = (freqScan/1e9).toFixed(4)
            break;
        default:
            freqScanString = freqScan.toString()
        }
        componentNotCompleted = true

        mainWindows.onTemperatureChanged.connect(function(value){
            cpuDatatemperature = value
        })

        mainWindows.addNewProfile.connect(function(value){
            addNewProfile(value)
        })

        scanpage.width = screenrotation==270 ? 1195 : 1920
        scanpage.height  = screenrotation==270 ? 400 : 1080
    }

    function addNewProfile(value){
        // var msg = JSON.stringify(value)
        // console.log("msg:",msg," name:",value.name)
        let uuid = ""
        if (modifyPreset == false)
            uuid = mainWindows.generateGUID().replace(/[{}]/g, "")
        const newPreset = value

        nameDialog.generatedPresetId = uuid
        nameDialog.pendingPresetObject = newPreset

        nameDialog.pendingPresetObject.name = value.name
        configManager.addOrModifyPreset(nameDialog.generatedPresetId, nameDialog.pendingPresetObject)

        console.log("generatedPresetId:",nameDialog.generatedPresetId," pendingPresetObject:",nameDialog.pendingPresetObject," presetNameField.text.trim()",presetNameField.text.trim())
        // Reload and update UI
        let presets = configManager.getPresetsAsList()
        radioMemList.clear()
        for (let i = 0; i < presets.length; i++) {
            radioMemList.append(presets[i])
        }
        configManager.saveToFile("/var/lib/openwebrx/preset.json");
        if(modifyPreset){
            mainWindows.editCardWebSlot(nameDialog.generatedPresetId,JSON.stringify(nameDialog.pendingPresetObject))
        }
        else{
            mainWindows.addCardWebSlot(nameDialog.generatedPresetId)
        }

        modifyPreset = false
    }

    function updateFrequency()
    {
        var str = freqScan.toString()
        // console.log("freqScan",freqScan," freqUnit",freqUnit)
        switch(freqUnit)
        {
        case "Hz":
            freqScanString = freqScan.toString()
            break;
        case "kHz":
            freqScanString = (freqScan/1e3).toFixed(3)
            break;
        case "MHz":
            freqScanString = (freqScan/1e6).toFixed(6)
            break;
        case "GHz":
            freqScanString = (freqScan/1e9).toFixed(9)
            break;
        default:
            freqScanString = freqScan.toString()
        }
    }
    RowLayout {
        y: 4
        height: 65
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 4
        anchors.rightMargin: 8


        Rectangle {
            id: rectangle
            color: gpiokeyProfile == 2 ? "#A0000000" : "#50000000"
            border.width: gpiokeyProfile == 2 ? 1 : 0
            border.color: "#80ffffff"
            Layout.fillHeight: true
            clip: true
            Layout.preferredWidth: 480

            ToolButton {
                id: toolButtonFUnit
                x: 450
                width: 65
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 35

                Rectangle {
                    color: "#00ffffff"
                    border.color: "#ffffff"
                    border.width: 0
                    anchors.fill: parent
                    Label {
                        width: 40
                        color: "#aaaaaa"
                        text: freqUnit
                        anchors.fill: parent
                        horizontalAlignment: Text.AlignLeft
                        verticalAlignment: Text.AlignBottom
                        font.pointSize: 20
                        anchors.topMargin: 0
                        anchors.bottomMargin: 0
                        font.bold: false
                    }
                }
                onClicked: {
                    indexGpiokeyProfile = 4
                    gpiokeyProfile = 2
                }
            }

            TextField {
                id: freqEdit
                height: 65
                color: freqEditColor
                text: freqScanString
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                font.pixelSize: 60
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignTop
                topPadding: 0
                bottomPadding: -20
                anchors.topMargin: 0
                anchors.rightMargin: 72
                placeholderText: "30 - 3200"
                validator: DoubleValidator {
                    bottom: 30.0
                    top: 3200.0
                    notation: DoubleValidator.StandardNotation
                }
                inputMethodHints: Qt.ImhDigitsOnly
                onFocusChanged: {

                }
                onCursorVisibleChanged: {
                    keyfreqEdit = cursorVisible
                    if (cursorVisible)
                    {
                        if(focus) selectAll()
                        console.log("onFocusChanged",focus,text)
                    }
                    focus = cursorVisible
                }

                onAccepted: {
                    focus = false
                    if ((freqScan >= freqMin) & (freqScan <= freqMax))
                    {
                        console.log("onAccepted setManualOffset::",freqScan)
                        spectrumGLPlot.setManualOffset(freqScan)
                    }
                }
                onTextChanged: {
                    if (componentNotCompleted == false) return
                    if (freqEdit.text != "")
                    {
                        switch (freqUnit)
                        {
                        case "Hz":
                            freqScan = parseInt(freqEdit.text)
                            break
                        case "kHz":
                            freqScan = parseFloat(freqEdit.text) * 1e3
                            break
                        case "MHz":
                            freqScan = parseFloat(freqEdit.text) * 1e6
                            break
                        case "GHz":
                            freqScan = parseFloat(freqEdit.text) * 1e9
                            break
                        }
                        // console.log("onTextChanged",freqScan, freqMin, freqMax)
                        if ((freqScan >= freqMin) & (freqScan <= freqMax))
                        {
                            freqEditColor = "#aaaaaa"
                        }
                        else
                        {
                            freqEditColor = "#ff5555"
                            console.log("frequency out of range")
                        }
                        indexGpiokeyProfile = 4
                        gpiokeyProfile = 2
                    }
                }
            }
        }
        Dialog {
            id: nameDialog
            modal: true
            focus: true
            visible: false

            // ✅ ไม่ใช้ header/title ของ Dialog (ให้เป็นกล่องเดียว)
            title: ""
            standardButtons: Dialog.NoButton
            x: (parent.width/2)-(width/2)
            y: (parent.height/2)+width/2
            // anchors.centerIn: parent

            width: Math.min(parent.width - 60, 520)
            height: 200

            property string generatedPresetId: ""
            property var pendingPresetObject: ({})

            // (Qt บางเวอร์ชันมี header ของ Dialog) — ใส่ไว้ไม่เสียหาย
            header: null
            footer: null

            background: Rectangle {
                radius: 22               // ✅ โค้งทั้งกล่อง
                color: "#0B1220"
                border.color: "#223049"
                border.width: 1
            }

            onOpened: {
                presetNameField.forceActiveFocus()
                presetNameField.selectAll()
            }

            function doOk() {
                if (presetNameField.text.trim() === "")
                    return

                // ✅ logic เดิมทั้งหมด
                pendingPresetObject.name = presetNameField.text.trim()
                configManager.addOrModifyPreset(generatedPresetId, pendingPresetObject)

                let presets = configManager.getPresetsAsList()
                radioMemList.clear()
                for (let i = 0; i < presets.length; i++) {
                    radioMemList.append(presets[i])
                }

                configManager.saveToFile("/var/lib/openwebrx/preset.json")

                if (modifyPreset) {
                    mainWindows.editCardWebSlot(generatedPresetId, JSON.stringify(pendingPresetObject))
                } else {
                    mainWindows.addCardWebSlot(generatedPresetId)
                }

                modifyPreset = false
                nameDialog.close()
            }

            contentItem: Item {
                anchors.fill: parent
                anchors.margins: 20

                Column {
                    anchors.fill: parent
                    spacing: 14

                    // ✅ Title อยู่ “ในกล่องเดียวกัน” (ไม่แยก header)
                    Text {
                        text: "Enter Preset Name"
                        color: "#F1F5F9"
                        font.pixelSize: 20
                        font.bold: true
                    }

                    // ✅ TextField โค้ง เนียน
                    TextField {
                        id: presetNameField
                        placeholderText: "Enter preset name"
                        width: parent.width
                        height: 46

                        font.pixelSize: 16
                        color: "#E5E7EB"
                        placeholderTextColor: "#93A4B8"

                        leftPadding: 14
                        rightPadding: 14
                        topPadding: 10
                        bottomPadding: 10
                        verticalAlignment: Text.AlignVCenter

                        background: Rectangle {
                            radius: 14
                            color: "#0F172A"
                            border.width: 1
                            border.color: presetNameField.activeFocus ? "#5A6A84" : "#334155"
                        }

                        Keys.onReturnPressed: nameDialog.doOk()
                        Keys.onEnterPressed:  nameDialog.doOk()
                        Keys.onEscapePressed: nameDialog.close()
                    }

                    Item { height: 2 }

                    Row {
                        spacing: 14
                        anchors.right: parent.right

                        Item {
                            id: cancelBtn
                            width: 140
                            height: 40

                            property bool hovered: cancelMouse.containsMouse
                            property bool pressed: cancelMouse.pressed

                            Rectangle {
                                anchors.fill: parent
                                radius: height / 2
                                color: cancelBtn.pressed
                                       ? "#1E293B"
                                       : (cancelBtn.hovered ? "#0F172A" : "transparent")
                                border.color: cancelBtn.hovered ? "#94A3B8" : "#3B4B63"
                                border.width: 1
                            }

                            Text {
                                anchors.centerIn: parent
                                text: "CANCEL"
                                color: cancelBtn.hovered ? "#FFFFFF" : "#F2F6FF"
                                font.pixelSize: 14
                                font.bold: true
                            }

                            MouseArea {
                                id: cancelMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: nameDialog.close()
                            }
                        }


                        Item {
                            id: okBtn
                            width: 140
                            height: 40

                            property bool hovered: okMouse.containsMouse
                            property bool pressed: okMouse.pressed

                            Rectangle {
                                anchors.fill: parent
                                radius: height / 2
                                color: okBtn.pressed
                                       ? "#064E3B"
                                       : (okBtn.hovered ? "#0F766E" : "transparent")
                                border.color: okBtn.hovered ? "#2DD4BF" : "#3B4B63"
                                border.width: 1
                            }

                            Text {
                                anchors.centerIn: parent
                                text: "OK"
                                color: okBtn.hovered ? "#ECFEFF" : "#2DD4BF"
                                font.pixelSize: 14
                                font.bold: true
                            }

                            MouseArea {
                                id: okMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: nameDialog.doOk()
                            }
                        }

                    }
                }
            }
        }


        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 76

            ToolButton {
                id: toolButtonScaner
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 35
                onClicked: {
                    openDrawer(4)
                    // drawerScanerOption.open()
                }
                Rectangle {
                    color: "#aa009688"
                    radius: 5
                    border.color: "#ffffff"
                    border.width: 0
                    anchors.fill: parent
                    Label {
                        width: 40
                        text: "RF Scan"
                        anchors.fill: parent
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.bold: false
                        anchors.topMargin: 0
                        anchors.bottomMargin: 0
                        font.pixelSize: 13
                    }
                }
            }

            ToolButton {
                id: toolButtonAnalogDigital
                Layout.fillWidth: true
                onClicked: {
                    // drawerReceiverOption.open()
                    openDrawer(3)
                }
                Layout.fillHeight: true
                Layout.preferredHeight: 35
                Rectangle {
                    color: buttonColor
                    radius: 5
                    border.color: "#ffffff"
                    border.width: 0
                    anchors.fill: parent
                    Label {
                        width: 40
                        text: receiverMode.get(scanReceiverModeSelected).mode
                        anchors.fill: parent
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.bold: false
                        anchors.topMargin: 0
                        anchors.bottomMargin: 0
                        font.pixelSize: 13
                    }
                }
            }


            ToolButton {
                id: toolButtonModSelect
                Layout.fillWidth: true
                onClicked: {
                    // drawerReceiverOption.open()
                    openDrawer(3)
                }
                Layout.fillHeight: true
                Layout.preferredHeight: 35
                Rectangle {
                    color: buttonColor
                    radius: 5
                    border.color: "#ffffff"
                    border.width: 0
                    anchors.fill: parent
                    Label {
                        width: 40
                        text: receiverMode.get(scanReceiverModeSelected).name
                        anchors.fill: parent
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.bold: false
                        anchors.topMargin: 0
                        anchors.bottomMargin: 0
                        font.pixelSize: 13
                    }
                }
            }


            ToolButton {
                id: toolButtonBandwidth
                Layout.fillWidth: true
                onClicked: {
                    // drawerReceiverOption.open()
                    console.log("bandwidth:",(spectrumGLPlot.high_cut - spectrumGLPlot.low_cut) > 1000 ? ((spectrumGLPlot.high_cut - spectrumGLPlot.low_cut)/1e3).toFixed(1) + "kHz" : (spectrumGLPlot.high_cut - spectrumGLPlot.low_cut).toFixed(0) + "Hz")
                    openDrawer(3)
                }
                Layout.fillHeight: true
                Layout.preferredHeight: 35
                Rectangle {
                    color: buttonColor
                    radius: 5
                    border.color: "#ffffff"
                    border.width: 0
                    anchors.fill: parent
                    Label {
                        width: 40
                        text: (spectrumGLPlot.high_cut - spectrumGLPlot.low_cut) > 1000 ? ((spectrumGLPlot.high_cut - spectrumGLPlot.low_cut)/1e3).toFixed(1) + "kHz" : (spectrumGLPlot.high_cut - spectrumGLPlot.low_cut).toFixed(0) + "Hz"
                        anchors.fill: parent
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.bold: false
                        anchors.topMargin: 0
                        anchors.bottomMargin: 0
                        font.pixelSize: 13
                    }
                }
            }
            ToolButton
            {
                id: toolButtonSql
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 35
                Rectangle {
                    color: gpiokeyProfile == 3 ? buttonColorRotary : buttonColor
                    radius: 5
                    border.color: "#ffffff"
                    border.width: 0
                    anchors.fill: parent
                    Label {
                        width: 40
                        text: "SQL\n" + ((scanSqlLevel-255)/2).toFixed(1)+" dB"
                        anchors.fill: parent
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.bottomMargin: 0
                        font.pixelSize: 13
                        font.bold: false
                        anchors.topMargin: 0
                    }
                }
                onClicked: {
                    indexGpiokeyProfile = 3
                    gpiokeyProfile = 3
                    // drawerSql.open()
                    openDrawer(2)
                }
            }

            ToolButton
            {
                id: toolButtonVolSoftware
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 35
                Rectangle {
                    color: gpiokeyProfile == 5 ? buttonColorRotary : buttonColor
                    radius: 5
                    border.color: "#ffffff"
                    border.width: 0
                    anchors.fill: parent
                    Label {
                        property string volText: scanMuteOn ? "Volume\nMute" : "Volume\n" + scanAudioLevel +" %"
                        width: 40
                        text: volText
                        anchors.fill: parent
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.bottomMargin: 0
                        font.pixelSize: 13
                        font.bold: false
                        anchors.topMargin: 0
                    }
                }
                onClicked: {
                    indexGpiokeyProfile = 2
                    gpiokeyProfile = 5
                    // drawerVolume.open()
                    openDrawer(5)
                }
            }

            ToolButton
            {
                id: toolButtonPhone
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 35
                Rectangle {
                    color: gpiokeyProfile == 1 ? buttonColorRotary : buttonColor
                    radius: 5
                    border.color: "#ffffff"
                    border.width: 0
                    anchors.fill: parent
                    Label {
                        property string phoneText: phoneMuteOn ? "Phone\nMute" : "Phone\n" + ((scanVolLevelHeadphone-255)/2).toFixed(1) +" dB"
                        width: 40
                        text: phoneText
                        anchors.fill: parent
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.bottomMargin: 0
                        font.pixelSize: 12
                        font.bold: false
                        anchors.topMargin: 0
                    }
                }
                onClicked:{
                    indexGpiokeyProfile = 1
                    gpiokeyProfile = 1
                    // drawerVolume.open()
                    openDrawer(1)
                }
            }

            ToolButton
            {
                id: toolButtonVol
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 35
                Rectangle {
                    color: gpiokeyProfile == 0 ? buttonColorRotary : buttonColor
                    radius: 5
                    border.color: "#ffffff"
                    border.width: 0
                    anchors.fill: parent
                    Label {
                        property string speakerText: "Speaker\n" + ((scanVolLevel-255)/2).toFixed(1) +" dB"
                        width: 40
                        text: speakerText
                        anchors.fill: parent
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.bottomMargin: 0
                        font.pixelSize: 13
                        font.bold: false
                        anchors.topMargin: 0
                    }
                }
                onClicked: {
                    indexGpiokeyProfile = 0
                    gpiokeyProfile = 0
                    // drawerVolume.open()
                    openDrawer(1)
                }
            }

            ToolButton {
                id: toolButtonNewPreset
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 35
                Rectangle {
                    color: "#aa009688"
                    radius: 5
                    border.color: "#ffffff"
                    border.width: 0
                    anchors.fill: parent
                    Image {
                        id: image
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        anchors.topMargin: 8
                        anchors.bottomMargin: 8
                        source: modifyPreset ? "images/save2.png" : "images/newfmradio.png"
                        fillMode: Image.PreserveAspectFit
                    }
                }
                onClicked: {
                    // console.log("toolButtonNewPreset onClick:",modifyPreset)
                    let uuid = modifyPresetId
                    if (modifyPreset == false)
                        uuid = mainWindows.generateGUID().replace(/[{}]/g, "")
                    const newPreset = {
                        "name": "",  // Will be filled in Dialog
                        "low_cut": currentLowcut,
                        "high_cut": currentHighcut,
                        "center_freq": currentCenterFreq,
                        "offset_freq": currentOffsetFreq,
                        "mod": receiverMode.get(scanReceiverModeSelected).text,
                        "dmr_filter": 3,
                        "audio_service_id": 0,
                        "squelch_level": currentSqlLevel,
                        "secondary_mod": false
                    }

                    nameDialog.generatedPresetId = uuid
                    nameDialog.pendingPresetObject = newPreset
                    presetNameField.text = ""  // clear last input
                    if (modifyPreset)
                        presetNameField.text = modifyPresetName
                    nameDialog.open()
                }
            }


            ToolButton {
                id: toolButtonRec
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 35
                property bool scanRecOn: false
                property real blinkOpacity: 0.6
                Component.onCompleted: {
                    mainWindows.onRecStatusChanged.connect(function(value){
                        scanRecOn = value
                    })
                }

                onScanRecOnChanged: {
                    if (scanRecOn) {
                        blinkOpacity = 1.0;
                        blinkTimer.start();
                    } else {
                        blinkOpacity = 0.6;
                        blinkTimer.stop();
                    }
                }
                Rectangle {
                    color: "#aa009688"
                    radius: 5
                    border.color: "#ffffff"
                    border.width: 0
                    anchors.fill: parent

                    Image {
                        id: image1
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        anchors.topMargin: 8
                        anchors.bottomMargin: 8
                        source: toolButtonRec.scanRecOn ? "images/recOn.png" : "images/recOff.png"
                        fillMode: Image.PreserveAspectFit
                        opacity: toolButtonRec.scanRecOn ? toolButtonRec.blinkOpacity : 0.6
                    }

                    // Blinking logic


                    Timer {
                        id: blinkTimer
                        interval: 500
                        running: toolButtonRec.scanRecOn
                        repeat: true
                        onTriggered: {
                            toolButtonRec.blinkOpacity = (toolButtonRec.blinkOpacity === 1.0) ? 0.3 : 1.0;
                        }
                    }
                }

            }
            Rectangle {
                id : cputempCard
                color: "#aa009688"
                radius: 5
                border.color: "#80000000"
                border.width: 0
                Layout.preferredWidth: 80
                Layout.fillHeight: true
                property real temperature: cpuDatatemperature
                onTemperatureChanged: cputempCanvas.requestPaint()
                Text {
                    anchors.centerIn: parent
                    text: cpuDatatemperature + "°C"
                    font.pixelSize: 16
                    anchors.verticalCenterOffset: 7
                    color: "white"
                }

                // Optional: Visual temperature bar (circle fill)
                Canvas {
                    id: cputempCanvas
                    anchors.fill: parent
                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)

                        const temp = cpuDatatemperature
                        const maxTemp = 100
                        const percentage = Math.min(temp / maxTemp, 1.0)

                        const barWidth = width - 20
                        const barHeight = 6
                        const barX = 10
                        const barY = 10

                        // Background bar
                        ctx.fillStyle = "#444"
                        ctx.fillRect(barX, barY, barWidth, barHeight)

                        // Foreground temperature bar
                        ctx.fillStyle = temp > 70 ? "red" : (temp > 50 ? "orange" : "limegreen")
                        ctx.fillRect(barX, barY, barWidth * percentage, barHeight)
                    }
                }

                // Timer {
                //     id: cputempCardTimer
                //     repeat: false
                //     running: true
                //     interval: 10000
                //     onTriggered: {
                //         cputempCard.opacity = 0.5
                //     }
                // }
                // MouseArea {
                //     z:100
                //     anchors.fill: parent
                //     onClicked: {
                //         cputempCardTimer.restart()
                //         cputempCard.opacity = 1
                //     }
                // }
                // Behavior on opacity {
                //     NumberAnimation { duration: 400; easing.type: Easing.InOutQuad }
                // }

            }


            spacing: 2
        }
    }

    SpectrumGLPlot {
        id: spectrumGLPlot
        anchors.fill: parent
        anchors.topMargin: 70
    }

    MyDrawer {
        id: drawer           // มีแค่ตัวเดียว
    }

    function openDrawer(which) {
        drawer.open(which)   // which: 1=Volume, 2=SQL, 3=ReceiveMode, 4=FindBands
    }

    function closeDrawer() {
        drawer.close()   // which: 1=Volume, 2=SQL, 3=ReceiveMode, 4=FindBands
    }

    // // ปุ่ม:
    // toolButtonVol.onClicked:         openDrawer(1)
    // toolButtonSql.onClicked:         openDrawer(2)
    // toolButtonAnalogDigital.onClicked: openDrawer(3)
    // toolButtonModSelect.onClicked:     openDrawer(3)
    // toolButtonScaner.onClicked:        openDrawer(4)

    // MyDrawer {
    //     id: drawerVolume
    //     property bool opened: drawerItem.opened && itemShow == 1
    //     itemShow: 1
    //     function open() {
    //         console.log("open drawerVolume")
    //         drawerItem.open()
    //     }

    //     function close() {
    //         console.log("close drawerVolume")
    //         drawerItem.close()
    //     }
    // }

    // MyDrawer {
    //     id: drawerSql
    //     property bool opened: drawerItem.opened && itemShow == 2
    //     itemShow: 2
    //     function open() {
    //         console.log("open drawerSql")
    //         drawerItem.open()
    //     }

    //     function close() {
    //         console.log("close drawerSql")
    //         drawerItem.close()
    //     }
    // }

    // MyDrawer {
    //     id: drawerReceiverOption
    //     property bool opened: drawerItem.opened && itemShow == 3
    //     itemShow: 3
    //     function open() {
    //         console.log("open drawerReceiverOption")
    //         drawerItem.open()
    //     }

    //     function close() {
    //         console.log("close drawerReceiverOption")
    //         drawerItem.close()
    //     }
    // }

    // MyDrawer {
    //     id: drawerScanerOption
    //     property bool opened: drawerItem.opened && itemShow == 4
    //     itemShow: 4
    //     function open() {
    //         console.log("open drawerScanerOption")
    //         drawerItem.open()
    //     }

    //     function close() {
    //         console.log("close drawerScanerOption")
    //         drawerItem.close()
    //     }

    // }
}


