import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.0
import QtQuick.Controls.Material 2.4
import "ui"
Item {
    id: scanpage
    // Theme is injected by HomeDisplay/MainPage so the analyzer and all local
    // dialogs switch immediately and deterministically with the global shell.
    property bool darkMode: true
    Theme { id: hmiTheme; darkMode: scanpage.darkMode }
    // width: 1195
    // height: 400
    property real freqMin: 10e6
    property real freqMax: 3.8e9
    // Absolute receiver frequency shown in the large readout.
    // SpectrumGLPlot owns live updates through center + DSP offset.
    property real freqScan: Number(mainWindows.receiver_freq())
    property bool frequencyUiSyncing: false
    property bool keyfreqEdit: false
    property color buttonColor: hmiTheme.accent
    property color buttonColorRotary: hmiTheme.accentHover
    property color freqEditColor: hmiTheme.textSecondary
    property real frequencyUnitValueButton: frequencyUnitValue
    property bool componentNotCompleted: false
    readonly property bool compactCommandBar: width < 1160
    readonly property bool narrowCommandBar: width < 900
    readonly property int commandFontPixelSize: narrowCommandBar ? 8 : (compactCommandBar ? 10 : 12)

    // True only while this receiver/spectrum page is the active navigation page.
    // HomeDisplay owns the authoritative binding because it knows both StackViews.
    property bool runtimeActive: false

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
        // Unit changes are display-only. Never reinterpret the existing text
        // using the new unit because that changes the actual tuned frequency.
        if (componentNotCompleted)
            updateFrequency()
    }

    onFreqScanChanged: {
        if (componentNotCompleted && !keyfreqEdit)
            updateFrequency()
    }

    Component.onCompleted:
    {
        freqScan = Number(mainWindows.receiver_freq())
        updateFrequency()
        componentNotCompleted = true

        // Let StackView/AppShell own normal landscape geometry so resize and
        // navigation-rail changes remain live. Preserve only the legacy 270°
        // hardware geometry when that mode is explicitly active.
        if (screenrotation == 270) {
            scanpage.width = 1195
            scanpage.height = 400
        }
    }


    // R20.2 / R16.1 restore: let QML own/disconnect these handlers with this item.
    Connections {
        target: mainWindows
        ignoreUnknownSignals: true

        function onOnTemperatureChanged(value) {
            cpuDatatemperature = value
        }

        function onAddNewProfile(value) {
            addNewProfile(value)
        }
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
        frequencyUiSyncing = true
        switch(freqUnit)
        {
        case "Hz":
            freqScanString = Math.round(freqScan).toString()
            break
        case "kHz":
            freqScanString = (freqScan/1e3).toFixed(3)
            break
        case "MHz":
            freqScanString = (freqScan/1e6).toFixed(6)
            break
        case "GHz":
            freqScanString = (freqScan/1e9).toFixed(9)
            break
        default:
            freqScanString = freqScan.toString()
        }
        frequencyUiSyncing = false
    }
    // Phase 2 HMI: one command surface visually groups the tuned frequency
    // and the high-use receiver controls.  The existing ToolButton IDs and
    // handlers remain authoritative; this layer changes presentation only.
    Rectangle {
        id: radioCommandBarBackdrop
        x: 6
        y: 4
        width: parent.width - 12
        height: hmiTheme.radioCommandBarHeight
        radius: hmiTheme.radiusLg
        color: hmiTheme.panel
        border.width: 1
        border.color: hmiTheme.line
        z: 0
    }

    RowLayout {
        y: 8
        height: 64
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: hmiTheme.gapSm
        z: 2


        Rectangle {
            id: rectangle
            color: hmiTheme.input
            border.width: gpiokeyProfile == 2 || freqEdit.activeFocus ? 2 : 1
            border.color: gpiokeyProfile == 2 || freqEdit.activeFocus
                          ? hmiTheme.accentHover : hmiTheme.lineStrong
            radius: hmiTheme.radiusMd
            Layout.fillHeight: true
            clip: true
            Layout.preferredWidth: scanpage.narrowCommandBar ? 245
                                   : (scanpage.compactCommandBar ? 300 : hmiTheme.radioFrequencyCardWidth)

            Text {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.leftMargin: 10
                anchors.topMargin: 5
                visible: !scanpage.narrowCommandBar
                text: "RECEIVER"
                color: hmiTheme.muted
                font.pixelSize: 9
                font.bold: true
                font.letterSpacing: 0.8
                z: 2
            }

            ToolButton {
                id: toolButtonFUnit
                x: 450
                width: scanpage.narrowCommandBar ? 52 : 65
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 35

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 7
                    radius: hmiTheme.radiusSm
                    color: toolButtonFUnit.pressed ? hmiTheme.card : hmiTheme.cardAlt
                    border.color: toolButtonFUnit.hovered ? hmiTheme.accentHover : hmiTheme.line
                    border.width: 1

                    Label {
                        color: hmiTheme.accentHover
                        text: freqUnit
                        anchors.fill: parent
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 13
                        font.bold: true
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
                font.pixelSize: scanpage.narrowCommandBar ? 26 : (scanpage.compactCommandBar ? 32 : 42)
                font.family: "monospace"
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
                topPadding: 5
                bottomPadding: 0
                anchors.topMargin: 0
                anchors.rightMargin: scanpage.narrowCommandBar ? 58 : 72
                anchors.leftMargin: scanpage.narrowCommandBar ? 10 : 16
                placeholderText: "10 - 3200"
                background: Rectangle { color: "transparent" }
                validator: DoubleValidator {
                    bottom: 10.0
                    top: 3200.0
                    notation: DoubleValidator.StandardNotation
                }
                inputMethodHints: Qt.ImhDigitsOnly
                onActiveFocusChanged: {
                    keyfreqEdit = activeFocus
                    if (activeFocus) {
                        selectAll()
                        console.log("[FREQ-EDIT] begin", text)
                    } else if (componentNotCompleted) {
                        // Once editing ends, return ownership to the canonical
                        // numeric receiver state.
                        updateFrequency()
                    }
                }

                onAccepted: {
                    keyfreqEdit = false
                    focus = false
                    if ((freqScan >= freqMin) & (freqScan <= freqMax))
                    {
                        console.log("onAccepted setManualOffset::",freqScan)
                        spectrumGLPlot.setManualOffset(freqScan)
                    }
                    else{
                        console.log("onAccepted Error setManualOffset::",freqScan," freqMin:",freqMin," freqMax",freqMax)
                    }
                }
                onTextChanged: {
                    if (componentNotCompleted == false || frequencyUiSyncing) return
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
                            freqEditColor = hmiTheme.textSecondary
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
            x: Math.max(12, (parent.width - width) / 2)
            y: Math.max(12, (parent.height - height) / 2)
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
                color: hmiTheme.panel
                border.color: hmiTheme.lineStrong
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
                        color: hmiTheme.text
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
                        color: hmiTheme.text
                        placeholderTextColor: hmiTheme.muted

                        leftPadding: 14
                        rightPadding: 14
                        topPadding: 10
                        bottomPadding: 10
                        verticalAlignment: Text.AlignVCenter

                        background: Rectangle {
                            radius: 14
                            color: hmiTheme.input
                            border.width: 1
                            border.color: presetNameField.activeFocus ? hmiTheme.accent : hmiTheme.lineStrong
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
                                       ? hmiTheme.cardAlt
                                       : (cancelBtn.hovered ? hmiTheme.input : "transparent")
                                border.color: cancelBtn.hovered ? hmiTheme.accentHover : hmiTheme.lineStrong
                                border.width: 1
                            }

                            Text {
                                anchors.centerIn: parent
                                text: "CANCEL"
                                color: hmiTheme.text
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
                                       ? Qt.darker(hmiTheme.accent, 1.18)
                                       : (okBtn.hovered ? hmiTheme.accent : "transparent")
                                border.color: okBtn.hovered ? hmiTheme.accentHover : hmiTheme.accent
                                border.width: 1
                            }

                            Text {
                                anchors.centerIn: parent
                                text: "OK"
                                color: okBtn.hovered ? "#061514" : hmiTheme.accent
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
            Layout.preferredHeight: 60
            spacing: 5

            ToolButton {
                id: toolButtonScaner
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 35
                onClicked: {
                    openDrawer(4)
                    // drawerScanerOption.open()
                }
                hoverEnabled: true
                HmiControlTile {
                    anchors.fill: parent
                    darkMode: hmiTheme.darkMode
                    fontPixelSize: scanpage.commandFontPixelSize
                    text: "RF SCAN"
                    tone: "primary"
                    hovered: toolButtonScaner.hovered
                    pressed: toolButtonScaner.pressed
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
                hoverEnabled: true
                HmiControlTile {
                    anchors.fill: parent
                    darkMode: hmiTheme.darkMode
                    fontPixelSize: scanpage.commandFontPixelSize
                    text: receiverMode.get(scanReceiverModeSelected).mode
                    tone: "primary"
                    hovered: toolButtonAnalogDigital.hovered
                    pressed: toolButtonAnalogDigital.pressed
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
                hoverEnabled: true
                HmiControlTile {
                    anchors.fill: parent
                    darkMode: hmiTheme.darkMode
                    fontPixelSize: scanpage.commandFontPixelSize
                    text: receiverMode.get(scanReceiverModeSelected).name
                    tone: "primary"
                    hovered: toolButtonModSelect.hovered
                    pressed: toolButtonModSelect.pressed
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
                hoverEnabled: true
                HmiControlTile {
                    anchors.fill: parent
                    darkMode: hmiTheme.darkMode
                    fontPixelSize: scanpage.commandFontPixelSize
                    text: (spectrumGLPlot.high_cut - spectrumGLPlot.low_cut) > 1000
                          ? ((spectrumGLPlot.high_cut - spectrumGLPlot.low_cut)/1e3).toFixed(1) + " kHz"
                          : (spectrumGLPlot.high_cut - spectrumGLPlot.low_cut).toFixed(0) + " Hz"
                    tone: "primary"
                    hovered: toolButtonBandwidth.hovered
                    pressed: toolButtonBandwidth.pressed
                }
            }
            ToolButton
            {
                id: toolButtonSql
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: 35
                hoverEnabled: true
                HmiControlTile {
                    anchors.fill: parent
                    darkMode: hmiTheme.darkMode
                    fontPixelSize: scanpage.commandFontPixelSize
                    text: "SQL\n" + ((scanSqlLevel-255)/2).toFixed(1) + " dB"
                    tone: "primary"
                    active: gpiokeyProfile == 3
                    hovered: toolButtonSql.hovered
                    pressed: toolButtonSql.pressed
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
                hoverEnabled: true
                HmiControlTile {
                    anchors.fill: parent
                    darkMode: hmiTheme.darkMode
                    fontPixelSize: scanpage.commandFontPixelSize
                    text: scanMuteOn ? "VOLUME\nMUTE" : "VOLUME\n" + scanAudioLevel + " %"
                    tone: scanMuteOn ? "warning" : "primary"
                    active: gpiokeyProfile == 5
                    hovered: toolButtonVolSoftware.hovered
                    pressed: toolButtonVolSoftware.pressed
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
                hoverEnabled: true
                HmiControlTile {
                    anchors.fill: parent
                    darkMode: hmiTheme.darkMode
                    fontPixelSize: scanpage.commandFontPixelSize
                    text: phoneMuteOn ? "PHONE\nMUTE" : "PHONE\n" + ((scanVolLevelHeadphone-255)/2).toFixed(1) + " dB"
                    tone: phoneMuteOn ? "warning" : "primary"
                    active: gpiokeyProfile == 1
                    hovered: toolButtonPhone.hovered
                    pressed: toolButtonPhone.pressed
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
                hoverEnabled: true
                HmiControlTile {
                    anchors.fill: parent
                    darkMode: hmiTheme.darkMode
                    fontPixelSize: scanpage.commandFontPixelSize
                    text: scanMuteOn
                          ? "SPEAKER\nMUTE"
                          : "SPEAKER\n" + ((scanVolLevel-255)/2).toFixed(1) + " dB"
                    tone: scanMuteOn ? "warning" : "primary"
                    active: gpiokeyProfile == 0
                    hovered: toolButtonVol.hovered
                    pressed: toolButtonVol.pressed
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
                hoverEnabled: true
                HmiControlTile {
                    anchors.fill: parent
                    darkMode: hmiTheme.darkMode
                    fontPixelSize: scanpage.commandFontPixelSize
                    text: ""
                    tone: "neutral"
                    active: modifyPreset
                    hovered: toolButtonNewPreset.hovered
                    pressed: toolButtonNewPreset.pressed
                }
                Image {
                    id: image
                    anchors.centerIn: parent
                    width: 30
                    height: 30
                    source: modifyPreset ? "images/save2.png" : "images/newfmradio.png"
                    fillMode: Image.PreserveAspectFit
                }
                Text {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 3
                    text: modifyPreset ? "SAVE" : "PRESET"
                    color: hmiTheme.textSecondary
                    font.pixelSize: 8
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
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
                hoverEnabled: true

                // R20.4 RADIO UX1.4: REC indication follows the actual recorder
                // state reported by LogWatcher/Mainwindows, not SQL. SQL can gate
                // recording policy, but it is not itself proof that alsarecd is
                // currently in RECORD state.
                property bool scanRecOn: false
                property bool recActualOn: false
                property bool recExpectedOn: false
                property bool blinkPhaseOn: true
                property string recorderStateText: "UNKNOWN"

                function applyRecorderUiState(expectedOn, actualOn, stateText) {
                    recExpectedOn = expectedOn
                    recActualOn = actualOn
                    recorderStateText = stateText

                    var visibleActive = expectedOn || actualOn
                    if (scanRecOn !== visibleActive) {
                        scanRecOn = visibleActive
                        console.log("[REC-UI] expected=", expectedOn,
                                    "actual=", actualOn,
                                    "state=", stateText)
                    }
                }

                function syncRecorderState() {
                    if (typeof mainWindows !== "undefined"
                            && mainWindows) {
                        var actualOn = false
                        var expectedOn = false
                        var stateText = recorderStateText

                        if (typeof mainWindows.getRecActive === "function")
                            actualOn = mainWindows.getRecActive()
                        if (typeof mainWindows.getRecExpectedActive === "function")
                            expectedOn = mainWindows.getRecExpectedActive()
                        if (typeof mainWindows.getRecorderState === "function")
                            stateText = mainWindows.getRecorderState()

                        applyRecorderUiState(expectedOn, actualOn, stateText)
                    }
                }

                Component.onCompleted: syncRecorderState()

                Connections {
                    target: (typeof mainWindows !== "undefined")
                            ? mainWindows
                            : null

                    function onOnRecStatusChanged(active) {
                        // Backward-compatible actual-recorder signal. The main UI
                        // signal below carries both expected and actual state, but
                        // older builds may still emit only this one. Keep expected
                        // active from the current getter so SQL-open audio still
                        // shows as ARMED while the watchdog reasserts alsarecd.
                        var stateText = toolButtonRec.recorderStateText
                        var expectedOn = toolButtonRec.recExpectedOn
                        if (typeof mainWindows.getRecorderState === "function")
                            stateText = mainWindows.getRecorderState()
                        if (typeof mainWindows.getRecExpectedActive === "function")
                            expectedOn = mainWindows.getRecExpectedActive()

                        toolButtonRec.applyRecorderUiState(expectedOn,
                                                           active,
                                                           stateText)
                    }

                    function onRecorderUiStateChanged(expectedActive,
                                                       actualRecord,
                                                       state) {
                        toolButtonRec.applyRecorderUiState(expectedActive,
                                                           actualRecord,
                                                           state)
                    }
                }

                onScanRecOnChanged: {
                    console.log("[REC ICON] recorder active =", scanRecOn)
                    blinkPhaseOn = true
                }

                Rectangle {
                    color: toolButtonRec.recActualOn
                           ? (toolButtonRec.blinkPhaseOn ? hmiTheme.danger : Qt.darker(hmiTheme.danger, 1.18))
                           : (toolButtonRec.recExpectedOn
                              ? (toolButtonRec.blinkPhaseOn ? hmiTheme.warning : Qt.darker(hmiTheme.warning, 1.18))
                              : hmiTheme.cardAlt)
                    radius: hmiTheme.radiusSm
                    border.color: toolButtonRec.recActualOn
                                  ? hmiTheme.danger
                                  : (toolButtonRec.recExpectedOn ? hmiTheme.warning
                                                                 : (toolButtonRec.hovered ? hmiTheme.accentHover : hmiTheme.lineStrong))
                    border.width: toolButtonRec.scanRecOn || toolButtonRec.hovered ? 2 : 1
                    anchors.fill: parent

                    Row {
                        anchors.centerIn: parent
                        spacing: 6

                        Rectangle {
                            id: recLiveDot
                            width: 10
                            height: 10
                            radius: 5
                            anchors.verticalCenter: parent.verticalCenter
                            color: toolButtonRec.recActualOn ? "#FFFFFF"
                                  : (toolButtonRec.recExpectedOn ? "#2A1700" : hmiTheme.muted)
                            visible: toolButtonRec.scanRecOn
                            opacity: toolButtonRec.scanRecOn
                                     ? (toolButtonRec.blinkPhaseOn ? 1.0 : 0.28)
                                     : 0.0
                        }

                        Text {
                            id: recLiveText
                            anchors.verticalCenter: parent.verticalCenter
                            text: toolButtonRec.recActualOn ? "REC"
                                  : (toolButtonRec.recExpectedOn ? "ARM" : "REC")
                            color: toolButtonRec.scanRecOn ? "#FFFFFF" : hmiTheme.textSecondary
                            font.pixelSize: 12
                            font.bold: true
                            font.letterSpacing: 0.5
                        }
                    }

                    Timer {
                        id: blinkTimer
                        interval: 500
                        repeat: true
                        // Recorder feedback follows actual recorder state. Do not gate
                        // this by scanpage.runtimeActive; runtimeActive is page/scan
                        // ownership state and can be false while alsarecd is correctly
                        // recording and audio remains normal.
                        running: toolButtonRec.scanRecOn && scanpage.visible
                        onTriggered: {
                            toolButtonRec.blinkPhaseOn =
                                    !toolButtonRec.blinkPhaseOn
                        }
                    }
                }
            }
            Rectangle {
                id : cputempCard
                color: hmiTheme.cardAlt
                radius: hmiTheme.radiusSm
                border.color: cpuDatatemperature > 70 ? hmiTheme.danger
                              : (cpuDatatemperature > 50 ? hmiTheme.warning : hmiTheme.lineStrong)
                border.width: 1
                Layout.preferredWidth: 86
                Layout.fillHeight: true
                property real temperature: cpuDatatemperature
                onTemperatureChanged: cputempCanvas.requestPaint()
                Column {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 4
                    spacing: 1
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "TEMP"
                        font.pixelSize: 8
                        font.bold: true
                        color: hmiTheme.muted
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: Number(cpuDatatemperature).toFixed(1) + "°C"
                        font.pixelSize: 13
                        font.bold: true
                        color: cpuDatatemperature > 70 ? hmiTheme.danger
                               : (cpuDatatemperature > 50 ? hmiTheme.warning : hmiTheme.text)
                    }
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
        }
    }

    SpectrumGLPlot {
        id: spectrumGLPlot
        darkMode: hmiTheme.darkMode
        anchors.fill: parent
        anchors.topMargin: 82
        runtimeActive: scanpage.runtimeActive
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


