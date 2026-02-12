import QtQuick 2.15
import Qt.labs.settings 1.1
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
// import QtQuick.Dialogs 1.3
// import QtMultimedia 5.12
import Receiver 1.0
import WebSocketClient 1.0
import QtQuick.Layouts 1.0
import App1 1.0
Item {
    id: homeDisplay
    width: 1195
    height: 400
    anchors.fill: parent
    property int currentMemIdEdit: -1


//Mem Add And Edit
    property string memName: "Name"
    property string analogDigital : "A"
    property string currentModType: "AM"
    property int currentModIndex: 0
    property string bandwidth: "5.5k"
    property string showUnit: "Hz"
    property real frequency: 0
    property real adModeSelected: 0
    property real receiverModeSelected: 0
    property real bwSelected: 0
    property real unitSelected: 0

//MainRadioScanner
    property real freqScan: 13000000
    property string freqScanString: ""
    property string freqUnit: "MHz"
    property string scanBandwidth: bwModel.get(scanBwSelected).name
    property string scanSql: "SQL"
    property real scanAdModeSelected : receiverMode.get(scanReceiverModeSelected).modeID
    property real scanReceiverModeSelected: 0
    property real scanBwSelected: 00

    property real scanReceiverModeSelectedBackup: 0
    property real scanBwSelectedBackUp: 0
    property real scanSqlLevel: 4
    property real scanSqlMode: 0
    property bool scanSqlLevelAuto: scanSqlMode == 0 || scanSqlMode == 2
    property real scanVolLevel: 0
    property real scanVolLevelHeadphone: 0
    property real scanAudioLevel: 0

    property bool scanMuteOn: false
    property bool phoneMuteOn: false
    property bool scanRadioPause: false
    property bool initData: false
    property int gpiokeyProfile: 0
    property int indexGpiokeyProfile: -1
    property real rotaryStep: radioScanner.spectrumGLPlot.offsetSnapStep
    property var bwModel: getBwModelById(bwModelName)
    property var bwModelName : receiverMode.get(scanReceiverModeSelected).bw
    property real currentCenterFreq: radioScanner.spectrumGLPlot.centerFreq
    property real currentLowcut: radioScanner.spectrumGLPlot.low_cut
    property real currentHighcut: radioScanner.spectrumGLPlot.high_cut
    property real currentOffsetFreq: radioScanner.spectrumGLPlot.offsetFrequency
    property string currectmodString: receiverMode.get(scanReceiverModeSelected).name
    property real currectWaterfallMinDb: radioScanner.spectrumGLPlot.waterfallMinDb
    property real currectWaterfallMaxDb: radioScanner.spectrumGLPlot.waterfallMaxDb
    property real currentSqlLevel: (scanSqlLevel-255)/2
    property real currentVolumeLevel: 0
    property bool modifyPreset: false
    property string modifyPresetId: ""
    property string modifyPresetName: ""

    property var previousCurrentRx: ({})
    property var pendingCurrentRx: ({})


    property var pendingDSPParams: ({})
    property int pendingCenterFreq: 0

    property bool interlockUpdate: false
    property bool currentRxLoaded: false
    property bool cardUpdate: false

    Timer {
        id: rotaryStepTimer10
        running: false
        repeat: true
        interval: 50
        onTriggered: setRotaryStep()
    }

    onScanReceiverModeSelectedChanged: {
        currectmodString = receiverMode.get(scanReceiverModeSelected).text
        // console.log("scanReceiverModeSelected:", scanReceiverModeSelected,
                    // "=> currectmodString:", currectmodString)
        // เช่น
        // maybeUpdateCurrentRx()
    }

    onScanAudioLevelChanged:{
        wsClient.setVolumePercent(scanAudioLevel)
        if (mainWindows.getSqlLevel() !== scanSqlLevel){
            mainWindows.setSqlLevel(scanSqlLevel)
            mainWindows.sendmessage('{"type": "dspcontrol","params": {"squelch_level": '+((scanSqlLevel-255)/2).toFixed(1)+'}}')
            currentSqlLevel = (scanSqlLevel-255)/2
            console.log("initData scanSqlLevel::",scanSqlLevel)
            wsClient.setSpeakerVolumeMute(0)
        }
        // console.log("onScanAudioLevelChanged maybeUpdateCurrentRx")
        maybeUpdateCurrentRx()
    }

    // onCurrentLowcutChanged: {
    //     console.log("onCurrentLowcutChanged")
    //     if (currentRxLoaded){
    //         console.log("onCurrentLowcutChanged maybeUpdateCurrentRx")
    //         maybeUpdateCurrentRx()
    //     }
    // }
    // onCurrentCenterFreqChanged: {
    //     console.log("onCurrentCenterFreqChanged")
    //     if (currentRxLoaded){
    //         console.log("onCurrentCenterFreqChanged maybeUpdateCurrentRx")
    //         maybeUpdateCurrentRx()
    //     }
    // }
    // onCurrentHighcutChanged: {
    //     console.log("onCurrentHighcutChanged")
    //     if (currentRxLoaded){
    //         console.log("onCurrentHighcutChanged maybeUpdateCurrentRx")
    //         maybeUpdateCurrentRx()
    //     }
    // }
    // onCurrentOffsetFreqChanged: {
    //     console.log("onCurrentOffsetFreqChanged")
    //     if (currentRxLoaded){
    //         console.log("onCurrentOffsetFreqChanged maybeUpdateCurrentRx")
    //         maybeUpdateCurrentRx()
    //     }
    // }

    // onCurrentSqlLevelChanged: {
    //     console.log("onCurrentSqlLevelChanged")
    //     if (currentRxLoaded){
    //         console.log("onCurrentSqlLevelChanged maybeUpdateCurrentRx")
    //         maybeUpdateCurrentRx()
    //     }
    // }
    // onCurrectWaterfallMaxDbChanged: {
    //     console.log("onCurrentLowcutChanged")
    //     if (currentRxLoaded){
    //         console.log("onCurrentLowcutChanged maybeUpdateCurrentRx")
    //         maybeUpdateCurrentRx()
    //     }
    // }
    // onCurrectWaterfallMinDbChanged: {
    //     console.log("onCurrectWaterfallMinDbChanged")
    //     if (currentRxLoaded){
    //         console.log("onCurrectWaterfallMinDbChanged maybeUpdateCurrentRx")
    //         maybeUpdateCurrentRx()
    //     }
    // }


    onCurrentLowcutChanged: maybeUpdateCurrentRx()
    onCurrentCenterFreqChanged: maybeUpdateCurrentRx()
    onCurrentHighcutChanged: maybeUpdateCurrentRx()
    onCurrentOffsetFreqChanged: maybeUpdateCurrentRx()
    onCurrectmodStringChanged: maybeUpdateCurrentRx()
    onCurrentSqlLevelChanged: maybeUpdateCurrentRx()
    onCurrectWaterfallMaxDbChanged: maybeUpdateCurrentRx()
    onCurrectWaterfallMinDbChanged: maybeUpdateCurrentRx()


    function maybeUpdateCurrentRx() {
        // console.log("maybeUpdateCurrentRx mod:", currectmodString);
        // console.log("scanAudioLevel:",scanAudioLevel)
        const currentRx = {
            "low_cut": currentLowcut,
            "high_cut": currentHighcut,
            "center_freq": currentCenterFreq,
            "offset_freq": currentOffsetFreq,
            "mod": currectmodString,
            "dmr_filter": 3,
            "audio_service_id": 0,
            "squelch_level": currentSqlLevel,
            "secondary_mod": false,
            "waterfallMinDb": currectWaterfallMinDb,
            "waterfallMaxDb": currectWaterfallMaxDb,
            "scanVolLevelHeadphone" : scanVolLevelHeadphone,
            "scanVolLevel" : scanVolLevel,
            "scanAudioLevel" : scanAudioLevel
        }
        // console.log("currentRx =", JSON.stringify(currentRx))


        // Compare with previous. Simple JSON compare.
        if (JSON.stringify(currentRx) !== JSON.stringify(previousCurrentRx)) {
            // console.log("currentRx changed -> restart timer")
            pendingCurrentRx = currentRx
            updateCurrentRxTimer.restart()
            previousCurrentRx = currentRx
        } else {
            // console.log("currentRx unchanged -> no timer reset")
        }
    }

    function maybeUpdate(currentRx) {
        if (JSON.stringify(currentRx) !== JSON.stringify(previousCurrentRx)) {
            // console.log("currentRx changed -> restart timer")
            pendingCurrentRx = currentRx
            updateCurrentRxTimer.restart()
            previousCurrentRx = currentRx
        } else {
            // console.log("currentRx unchanged -> no timer reset")
        }
    }

    Timer {
        id: updateCurrentRxTimer
        interval: 5000  // 30 seconds
        repeat: false
        onTriggered: {
            configManager.setCurrentConfig(pendingCurrentRx)
            console.log("Updated currentRx:", JSON.stringify(pendingCurrentRx))
            configManager.saveToFile("/var/lib/openwebrx/preset.json")
            console.log("Updated and saved currentRx")
        }
    }

    function getBwModelById(bwId) {
        switch (bwId) {
        case "fmIFBW": return fmIFBW
        case "wfmIFBW": return wfmIFBW
        case "amIFBW": return amIFBW
        case "lsbIFBW": return lsbIFBW
        case "usbIFBW": return usbIFBW
        case "usbdIFBW": return usbdIFBW
        case "cwIFBW": return cwIFBW
        case "dmrIFBW": return dmrIFBW
        case "dstarIFBW": return dstarIFBW
        case "nxdnIFBW": return nxdnIFBW
        case "yfsIFBW": return yfsIFBW
        default:
            console.warn("Unknown BW model for ID:", bwId)
            return null
        }
    }

    function getReceiverIndex(searchText) {
        for (let i = 0; i < receiverMode.count; ++i) {
            // console.log("getReceiverIndex(searchText):",receiverMode.get(i).text," searchText",searchText)
            if (receiverMode.get(i).text.toUpperCase() === searchText.toUpperCase())
                return i;
        }
        return -1; // ไม่พบ
    }

    function setRotaryStep()
    {
        rotaryStep -= radioScanner.spectrumGLPlot.offsetSnapStep*5
        if (rotaryStep < radioScanner.spectrumGLPlot.offsetSnapStep)
        {
            rotaryStep = radioScanner.spectrumGLPlot.offsetSnapStep
            rotaryStepTimer10.stop()
        }
    }

    function rotaryEvent(val)
    {
        if(val === -1) //CW
        {
            if (gpiokeyProfile == 0) {
                var ctrl = radioScanner.drawerVolume.volumeCtrl

                // step = +1 หรือ -1 (ตามปุ่ม)
                var step = +1

                // --- 1) คำนวณค่าจาก source of truth ---
                var v    = scanVolLevel + step
                var minV = ctrl.volumeCtrlLevel.levelmin
                var maxV = ctrl.volumeCtrlLevel.levelmax

                if (v < minV) v = minV
                if (v > maxV) v = maxV

                // --- 2) อัปเดต source of truth (UI ทั้งระบบจะตาม) ---
                scanVolLevel = v

                // --- 3) อัปเดต property ใน drawer ให้ตรงทันที (กันกรณี binding ไม่แน่น) ---
                ctrl.ctrlLevel = v

                // --- 4) เปิด drawer ---
                radioScanner.drawerVolume.open(1)
            }
            else if (gpiokeyProfile == 1)
            {
                var ctrl = radioScanner.drawerVolume.volumeCtrl
                var step = +1   // หรือ -1 ถ้าเป็นปุ่มลด

                // --- 1) คำนวณจาก source of truth ---
                var v    = scanVolLevelHeadphone + step
                var minV = ctrl.volumeHeadphoneCtrlLevel.levelmin
                var maxV = ctrl.volumeHeadphoneCtrlLevel.levelmax

                if (v < minV) v = minV
                if (v > maxV) v = maxV

                // --- 2) อัปเดต source of truth ---
                scanVolLevelHeadphone = v

                // --- 3) อัปเดต drawer property ให้ UI ตามทัน ---
                ctrl.headphoneCtrlLevel = v

                // --- 4) เปิด drawer ---
                radioScanner.drawerVolume.open(1)
            }
            else if (gpiokeyProfile == 2)
            {
                radioScanner.spectrumGLPlot.setOffset(radioScanner.spectrumGLPlot.offsetFrequency + rotaryStep)
                if (rotaryStepTimer10.running)
                {
                    rotaryStep += radioScanner.spectrumGLPlot.offsetSnapStep*5
                    if (rotaryStep >= radioScanner.spectrumGLPlot.offsetSnapStep*1000)
                        rotaryStep = radioScanner.spectrumGLPlot.offsetSnapStep*1000
                }
                rotaryStepTimer10.start()
            }
            else if (gpiokeyProfile == 3)
            {
                radioScanner.drawerSql.sqlCtrl.ctrlLevel++;
                // console.log("gpiokeyProfile == 3::",radioScanner.drawerSql.sqlCtrl.ctrlLevel)
                if (radioScanner.drawerSql.sqlCtrl.ctrlLevel > radioScanner.drawerSql.sqlCtrl.volumeCtrlLevel.levelmax)
                    radioScanner.drawerSql.sqlCtrl.ctrlLevel = radioScanner.drawerSql.sqlCtrl.volumeCtrlLevel.levelmax;
                if (!interlockUpdate) {
                    interlockUpdate = true
                    scanSqlLevel = radioScanner.drawerSql.sqlCtrl.ctrlLevel
                    interlockUpdate = false
                }
                radioScanner.drawerSql.open(2)
            }
            else if (gpiokeyProfile == 5)
            {
                var ctrl = radioScanner.drawerVolume.volumeCtrl   // ใช้ drawer เดียว
                var step = +1   // หรือ -1

                // --- 1) คำนวณจาก source of truth ---
                var v = scanAudioLevel + step
                if (v < 0)   v = 0
                if (v > 100) v = 100

                // --- 2) อัปเดต source of truth ---
                scanAudioLevel = v

                // --- 3) อัปเดต drawer property ---
                ctrl.audioLevel = v

                // --- 4) เปิด drawer โหมด audio ---
                radioScanner.drawerVolume.open(5)
            }
        }
        else if(val === 1) //CCW
        {
            if (gpiokeyProfile == 0) {
                var ctrl = radioScanner.drawerVolume.volumeCtrl

                // step = +1 หรือ -1 (ตามปุ่ม)
                var step = -1

                // --- 1) คำนวณค่าจาก source of truth ---
                var v    = scanVolLevel + step
                var minV = ctrl.volumeCtrlLevel.levelmin
                var maxV = ctrl.volumeCtrlLevel.levelmax

                if (v < minV) v = minV
                if (v > maxV) v = maxV

                // --- 2) อัปเดต source of truth (UI ทั้งระบบจะตาม) ---
                scanVolLevel = v

                // --- 3) อัปเดต property ใน drawer ให้ตรงทันที (กันกรณี binding ไม่แน่น) ---
                ctrl.ctrlLevel = v

                // --- 4) เปิด drawer ---
                radioScanner.drawerVolume.open(1)
            }
            else if (gpiokeyProfile == 1)
            {
                var ctrl = radioScanner.drawerVolume.volumeCtrl
                var step = -1   // หรือ -1 ถ้าเป็นปุ่มลด

                // --- 1) คำนวณจาก source of truth ---
                var v    = scanVolLevelHeadphone + step
                var minV = ctrl.volumeHeadphoneCtrlLevel.levelmin
                var maxV = ctrl.volumeHeadphoneCtrlLevel.levelmax

                if (v < minV) v = minV
                if (v > maxV) v = maxV

                // --- 2) อัปเดต source of truth ---
                scanVolLevelHeadphone = v

                // --- 3) อัปเดต drawer property ให้ UI ตามทัน ---
                ctrl.headphoneCtrlLevel = v

                // --- 4) เปิด drawer ---
                radioScanner.drawerVolume.open(1)
            }
            else if (gpiokeyProfile == 2)
            {
                radioScanner.spectrumGLPlot.setOffset(radioScanner.spectrumGLPlot.offsetFrequency - rotaryStep)
                if (rotaryStepTimer10.running)
                {
                    rotaryStep += radioScanner.spectrumGLPlot.offsetSnapStep*10
                    if (rotaryStep == radioScanner.spectrumGLPlot.offsetSnapStep*1000)
                        rotaryStep = radioScanner.spectrumGLPlot.offsetSnapStep*1000
                }

                rotaryStepTimer10.start()
            }
            else if (gpiokeyProfile == 3)
            {
                radioScanner.drawerSql.sqlCtrl.ctrlLevel--;
                // console.log("gpiokeyProfile == 3::",radioScanner.drawerSql.sqlCtrl.ctrlLevel)
                if (radioScanner.drawerSql.sqlCtrl.ctrlLevel < radioScanner.drawerSql.sqlCtrl.volumeCtrlLevel.levelmin)
                    radioScanner.drawerSql.sqlCtrl.ctrlLevel = radioScanner.drawerSql.sqlCtrl.volumeCtrlLevel.levelmin;

                if (!interlockUpdate) {
                    interlockUpdate = true
                    scanSqlLevel = radioScanner.drawerSql.sqlCtrl.ctrlLevel
                    interlockUpdate = false
                }
                radioScanner.drawerSql.open(2)
            }
            else if (gpiokeyProfile == 5)
            {
                var ctrl = radioScanner.drawerVolume.volumeCtrl   // ใช้ drawer เดียว
                var step = -1   // หรือ -1

                // --- 1) คำนวณจาก source of truth ---
                var v = scanAudioLevel + step
                if (v < 0)   v = 0
                if (v > 100) v = 100

                // --- 2) อัปเดต source of truth ---
                scanAudioLevel = v

                // --- 3) อัปเดต drawer property ---
                ctrl.audioLevel = v

                // --- 4) เปิด drawer โหมด audio ---
                radioScanner.drawerVolume.open(5)
            }
        }
    }

    function getListIndexByName(datalist, name)
    {
        var dataIndex = 0
        for(var index; index<datalist.count; index++)
        {
            if (datalist.get(index).name === name)
            {
                break
            }
            dataIndex++
        }
        return dataIndex

    }
    function updateProfiles(value) {
        const newIds = value.map(v => v.id);

        // Remove non-permanent profiles not in the new list
        for (let i = profiles.count - 1; i >= 1; --i) {
            if (!newIds.includes(profiles.get(i).profileId)) {
                profiles.remove(i);
            }
        }

        // Update existing or append new
        for (let i = 0; i < value.length; ++i) {
            const incoming = value[i];
            let found = false;

            for (let j = 1; j < profiles.count; ++j) { // start from 1 to skip permanent
                if (profiles.get(j).profileId === incoming.id) {
                    profiles.set(j, {
                        profileId: incoming.id,
                        name: incoming.name,
                        isNew: false,
                        isPermanent: false
                    });
                    found = true;
                    break;
                }
            }

            if (!found) {
                profiles.append({
                    profileId: incoming.id,
                    name: incoming.name,
                    isNew: true,
                    isPermanent: false
                });
            }
        }

        // console.log("✅ Profiles synced. Count:", profiles.count);
    }

    function numOr(v, def) {
        // กัน undefined / null / "" / NaN
        var n = Number(v)
        return (v === undefined || v === null || v === "" || Number.isNaN(n)) ? def : n
    }

    function strOr(v, def) {
        return (v === undefined || v === null) ? def : String(v)
    }

    function loadCurrentConfig() {
        let config = configManager.getCurrentConfig() || {}

        // ---- doubles / numbers (กัน undefined) ----
        scanVolLevelHeadphone = numOr(config.scanVolLevelHeadphone, 50)   // <-- เลือก default ที่คุณต้องการ
        scanVolLevel          = numOr(config.scanVolLevel, 50)
        scanAudioLevel        = numOr(config.scanAudioLevel, 50)

        mainWindows.setSpeakerVolume(scanVolLevel)
        mainWindows.setHeadphoneVolume(scanVolLevelHeadphone)
        wsClient.setVolumePercent(scanAudioLevel)

        // waterfall levels (กัน undefined ด้วย)
        var wfMin = numOr(config.waterfallMinDb, -120)
        var wfMax = numOr(config.waterfallMaxDb, -20)
        radioScanner.spectrumGLPlot.updateWaterfallLevels(wfMin, wfMax)

        currectmodString = strOr(config.mod, "FM")
        initData = true

        // ---- dspcontrol params (ตัวเลขก็กันไว้) ----
        var dspcontrolParams = {
            type: "dspcontrol",
            params: {
                "low_cut":          numOr(config.low_cut, -5000),
                "high_cut":         numOr(config.high_cut, 5000),
                "offset_freq":      numOr(config.offset_freq, 0),
                "mod":              strOr(config.mod, "FM"),
                "dmr_filter":       !!config.dmr_filter,
                "audio_service_id": numOr(config.audio_service_id, 0),
                "squelch_level":    numOr(config.squelch_level, 0),
                "secondary_mod":    strOr(config.secondary_mod, "")
            }
        }

        if (mainWindows && typeof mainWindows.sendmessage === "function") {
            var cf = numOr(config.center_freq, 0)

            // ส่งความถี่ทันที (กัน config.center_freq เป็น undefined)
            mainWindows.sendmessage(
                '{"type":"setfrequency","params":{"frequency":' + cf + ',"key":"memagic"}}'
            )

            pendingDSPParams = dspcontrolParams
            pendingCenterFreq = cf
            sendDSPTimer.restart()
        } else {
            console.error("mainWindows or sendmessage() is not available")
        }
    }
    Timer {
        id: sendDSPTimer
        interval: 500  // 1 second
        repeat: false
        onTriggered: {
            mainWindows.sendmessage(JSON.stringify(pendingDSPParams))
            radioScanner.spectrumGLPlot.centerFreq = pendingCenterFreq
            radioScanner.spectrumGLPlot.low_cut = pendingDSPParams.params.low_cut
            radioScanner.spectrumGLPlot.high_cut = pendingDSPParams.params.high_cut
            radioScanner.spectrumGLPlot.offsetFrequency = pendingDSPParams.params.offset_freq
            scanSqlLevel = (pendingDSPParams.params.squelch_level * 2) + 255
            stackView.pop(null)
            listView.currentIndex = 0
            console.log("HomeDisplay sendDSPTimer::",scanSqlLevel)
            radioScanner.spectrumGLPlot.start_mod = pendingDSPParams.params.mod
        }
    }

    Timer {
        id: setOffset
        running: false
        repeat: false
        interval: 3000
        onTriggered: {

        }
    }

    Timer {
        id: loadCurrentRxConfig
        running: false
        repeat: false
        interval: 3000
        onTriggered: loadCurrentConfig()
    }

    function onOpenwebrxConnected()
    {
        console.log("onOpenwebrxConnected")
        loadCurrentRxConfig.start()
    }

    Component.onCompleted: {
        // scanVolLevel =  mainWindows.getSpeakerVolume1()
        // scanVolLevelHeadphone =   mainWindows.getHeadphoneVolume()
        // scanSqlLevel = mainWindows.getSqlLevel()
        // initData = true
        // console.log("Component.onCompleted",scanVolLevel,scanVolLevelHeadphone)
        // mainWindows.setSpeakerVolume(scanVolLevel)
        // mainWindows.setHeadphoneVolume(scanVolLevelHeadphone)


        mainWindows.updateListProfiles.connect(updateListProfiles)

        mainWindows.selectSpecificProfile.connect(function(value) {
            maybeUpdate(value)
        })

        mainWindows.deleteScanProfile.connect(function(value) {
            deleteScanProfile(value)
        })

        mainWindows.deleteAllPresets.connect(deleteAllPresets)

        mainWindows.deleteSpecificProfile.connect(function(value) {
            deleteSpecificProfile(value)
        })

        mainWindows.editSpecificProfile.connect(function(value) {
            editSpecificProfile(value)
        })

        mainWindows.updateProfiles.connect(function(value) {
            updateProfiles(value);
        });
        mainWindows.updateGPIOKeyProfiles.connect(function(value)
        {
            if (radioScanner.drawerSql.opened)
            {
                radioScanner.drawerSql.close()
            }
            else if (radioScanner.drawerVolume.opened)
            {
                radioScanner.drawerVolume.close()
            }
            else
            {
                indexGpiokeyProfile++
                if(indexGpiokeyProfile > 4) indexGpiokeyProfile=0;
                if(indexGpiokeyProfile === 0){
                    gpiokeyProfile = 0;
                }
                else if(indexGpiokeyProfile === 1){
                    gpiokeyProfile = 1;
                }
                else if(indexGpiokeyProfile === 2){
                    gpiokeyProfile = 5;
                }
                else if(indexGpiokeyProfile === 3){
                    gpiokeyProfile = 3;
                }
                else if(indexGpiokeyProfile === 4){
                    gpiokeyProfile = 2;
                }


                // console.log("gpiokeyProfile:",gpiokeyProfile)
                // console.log("indexGpiokeyProfile:",indexGpiokeyProfile)
            }
        });
        mainWindows.updateRotaryProfiles.connect(function(value) {
            rotaryEvent(value);
        });

        configManager.loadPresetsFromFile("/var/lib/openwebrx/preset.json")
        let presets = configManager.getPresetsAsList()

        for (let i = 0; i < presets.length; i++) {
            radioMemList.append(presets[i])
        }


        mainWindows.onOpenwebrxConnected.connect(onOpenwebrxConnected)
        homeDisplay.width = screenrotation == 270 ? 1280 : 1920
        homeDisplay.height = screenrotation == 270 ? 400 : 1080
    }


    function updateListProfiles(){
        radioMemList.clear()
        configManager.loadPresetsFromFile("/var/lib/openwebrx/preset.json")
        let presets = configManager.getPresetsAsList()

        for (let i = 0; i < presets.length; i++) {
            radioMemList.append(presets[i])
        }
        // console.log("updateListProfiles::",presets.length,radioMemList.length)
    }

    function deleteScanProfile(value){
        console.log("deleteScanProfile")
    }

    function deleteAllPresets() {
        // 1) เคลียร์ preset ทั้งหมดใน configManager
        if (typeof configManager.deleteAllPresets === "function") {
            configManager.deleteAllPresets()
        } else if (typeof configManager.setPresets === "function") {
            // fallback method: ตั้งเป็น array ว่าง
            configManager.setCurrentConfig([])
        } else {
            console.warn("[deleteAllPresets] configManager does not support deleteAllPresets or setPresets")
            return
        }

        // 2) เซฟลงไฟล์ preset.json
        configManager.saveToFile("/var/lib/openwebrx/preset.json")

        // 3) รีเฟรช QML ListModel (radioMemList)
        if (typeof configManager.getPresetsAsList === "function" &&
            radioMemList && typeof radioMemList.clear === "function")
        {
            radioMemList.clear()   // เพราะไม่มี presets เหลือ
        }

        // console.log("[deleteAllPresets] All presets deleted.")
    }


    function deleteSpecificProfile(value){
        configManager.deletePreset(value)
        configManager.saveToFile("/var/lib/openwebrx/preset.json")
        // รีเฟรช list ถ้าจำเป็น
        if (typeof configManager.getPresetsAsList === "function" && radioMemList && radioMemList.clear) {
            let presets = configManager.getPresetsAsList()
            radioMemList.clear()
            for (let i = 0; i < presets.length; i++) radioMemList.append(presets[i])
        }
        // console.log("updateListProfiles(value) = ",value," radioMemList ID:")
    }

    function editSpecificProfile(value){
        var obj;
        try {
            obj = JSON.parse(value);
        } catch (e) {
            // console.log("handleEditPresetMessage: invalid JSON", e);
            return;
        }

        if (!obj || obj.objectName !== "editPreset") {
            // ไม่ใช่ข้อความสำหรับ editPreset ก็ข้ามไป
            return;
        }

        var presetId = obj.presetId;
        if (!presetId || presetId === "") {
            // console.log("handleEditPresetMessage: missing presetId");
            return;
        }

        // สร้าง object preset แบบเดียวกับตอน add
        var presetObject = {
            id:               presetId,                      // ถ้า configManager ต้องการฟิลด์ id
            name:             obj.name || "",
            center_freq:      Number(obj.center_freq || 0),
            offset_freq:      Number(obj.offset_freq || 0),
            dmr_filter:       Number(obj.dmr_filter || 0),
            high_cut:         Number(obj.high_cut || 0),
            low_cut:          Number(obj.low_cut || 0),
            mod:              obj.mod || "wfm",
            squelch_level:    Number(obj.squelch_level || 0),
            audio_service_id: Number(obj.audio_service_id || 0),
            secondary_mod:    !!obj.secondary_mod
        }

        // เรียกใช้ฟังก์ชันเดิม addOrModifyPreset (จะ add หรือแก้ไขตาม presetId)
        configManager.addOrModifyPreset(presetId, presetObject)

        // console.log("handleEditPresetMessage success:");

        // Reload model ให้ UI อัพเดต
        var presets = configManager.getPresetsAsList()
        radioMemList.clear()
        for (var i = 0; i < presets.length; i++) {
            radioMemList.append(presets[i])
        }

        // เซฟลงไฟล์
        configManager.saveToFile("/var/lib/openwebrx/preset.json");
    }

    onScanVolLevelChanged: {
        if (initData) {
            if (mainWindows.getSpeakerVolume1() !== scanVolLevel){
                mainWindows.setSpeakerVolume(scanVolLevel)
                mainWindows.setSqlLevel(scanSqlLevel)
                mainWindows.sendmessage('{"type": "dspcontrol","params": {"squelch_level": '+((scanSqlLevel-255)/2).toFixed(1)+'}}')
                currentSqlLevel = (scanSqlLevel-255)/2
                console.log("initData scanSqlLevel::",scanSqlLevel)
                wsClient.setSpeakerVolumeMute(0)
            }
        }
        if (currentRxLoaded)
            maybeUpdateCurrentRx()
    }

    onScanVolLevelHeadphoneChanged: {
        if (initData) {
            if (mainWindows.getHeadphoneVolume() !== scanVolLevelHeadphone){
                mainWindows.setHeadphoneVolume(scanVolLevelHeadphone)
                mainWindows.setSqlLevel(scanSqlLevel)
                mainWindows.sendmessage('{"type": "dspcontrol","params": {"squelch_level": '+((scanSqlLevel-255)/2).toFixed(1)+'}}')
                currentSqlLevel = (scanSqlLevel-255)/2
                console.log("initData scanSqlLevel::",scanSqlLevel)
                wsClient.setSpeakerVolumeMute(0)
            }
        }
        if (currentRxLoaded)
            maybeUpdateCurrentRx()
    }

    onScanSqlLevelChanged: {
        if (interlockUpdate) return

        // sync UI control ให้ตามค่าจริงที่เปลี่ยนมาจากระบบ
        if (radioScanner && radioScanner.drawerSql && radioScanner.drawerSql.sqlCtrl) {
            if (radioScanner.drawerSql.sqlCtrl.ctrlLevel !== scanSqlLevel) {
                interlockUpdate = true
                radioScanner.drawerSql.sqlCtrl.ctrlLevel = scanSqlLevel
                interlockUpdate = false
                console.log("radioScanner scanSqlLevel::",scanSqlLevel)
                wsClient.setSpeakerVolumeMute(0)
            }
        }
        // console.log("onScanSqlLevelChanged",scanSqlLevel)
        if (initData){
            if (mainWindows.getSqlLevel() !== scanSqlLevel){
                mainWindows.setSqlLevel(scanSqlLevel)
                mainWindows.sendmessage('{"type": "dspcontrol","params": {"squelch_level": '+((scanSqlLevel-255)/2).toFixed(1)+'}}')
                currentSqlLevel = (scanSqlLevel-255)/2
                console.log("initData scanSqlLevel::",scanSqlLevel)
                wsClient.setSpeakerVolumeMute(0)
            }
        }

        if (currentRxLoaded)
            maybeUpdateCurrentRx()
    }

    onGpiokeyProfileChanged: {

    }

    ReceiverConfigManager {
        id: configManager
        onPresetsChanged: console.log("Presets updated!")
        onCurrentConfigChanged: console.log("Current config changed:", newConfig)
    }

    function reopenList(){
        radioMemList.clear()
        configManager.loadPresetsFromFile("/var/lib/openwebrx/preset.json")
        let presets = configManager.getPresetsAsList()

        for (let i = 0; i < presets.length; i++) {
            radioMemList.append(presets[i])
        }
    }

    ListModel {
        id: radioMemList
        // ListElement {
        //     profileId: "new-preset"
        //     name: "New Preset"
        //     isNew: false
        //     isPermanent: true
        // }
    }
    ListModel {
        id: profiles
        // ListElement {
        //     profileId: "new-profile"
        //     name: "New Profile"
        //     isNew: false
        //     isPermanent: true
        // }
    }
    Label {
        id: label
        text: qsTr("Receive mode")
        anchors.top: parent.top
        font.pointSize: 16
        anchors.topMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
    }
    ListModel {
        id: updateListModel
        ListElement { index: 0;  name: "Cancel"}
        ListElement { index: 1;  name: "Update"}
    }



    Rectangle {
        id: rectangle1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 60
        height: parent.height - 60
        color: "#23404d"
        // anchors.fill: parent
        StackView
        {
            id: stackView
            // anchors.left: parent.left
            // anchors.right: parent.right
            // anchors.top: parent.top
            // anchors.bottom: parent.bottom
            anchors.fill: parent
            anchors.leftMargin: screenrotation == 270 ? 85 : 0

            initialItem: RadioScanner {
                id: radioScanner
                objectName: "radioScanner"
            }
        }
        ListView
        {
            id: listView
            currentIndex: 0
            onCurrentIndexChanged: {
                if (!((currentIndex == 0) || (currentIndex == -1))){
                    stackView.push(listmodel.get(currentIndex).source)
                    stackView.currentItem.objectName=listmodel.get(currentIndex).name
                }
                else if (currentIndex == 0)
                {
                    stackView.pop(null)
                    stackView.currentItem.objectName="radioScanner"
                }
                else
                {

                }
            }
            model: listmodel
        }

    }
    // =========================================================
    // Confirm Dialog (Reusable)
    // =========================================================
    Dialog {
        id: clearPresetActionDialog
        modal: true
        focus: true
        standardButtons: Dialog.NoButton
        width: Math.min(parent.width - 60, 520)
        height: 150

        x: (parent.width - width) / 2
        y: (parent.height - height) / 2

        // ====== CONFIGURABLE TEXT ======
        property string titleText:   "Confirm delete"
        property string messageText: "Delete Memory All"
        property int messageInt: 0

        // callback ตอนกด DELETE
        property var onConfirm: null

        background: Rectangle {
            radius: 18
            color: "#0E1520"
            border.color: "#1C2A3D"
            border.width: 1
        }

        contentItem: Item {
            anchors.fill: parent
            anchors.margins: 18

            Column {
                anchors.fill: parent
                spacing: 16

                // ===== Title =====
                Text {
                    text: clearPresetActionDialog.titleText
                    color: "#FFFFFF"
                    font.pixelSize: 20
                    font.bold: true
                }

                // ===== Message =====
                Text {
                    text: clearPresetActionDialog.messageText
                    color: "#BFD0E6"
                    font.pixelSize: 24
                    wrapMode: Text.WordWrap
                }

                Item { height: 6 }

                // ===== Buttons =====
                Row {
                    spacing: 12
                    anchors.right: parent.right

                    // ---------- CANCEL ----------
                    Item {
                        width: 140
                        height: 40

                        Rectangle {
                            anchors.fill: parent
                            radius: height / 2
                            color: "transparent"
                            border.color: "#3B4B63"
                            border.width: 1
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "CANCEL"
                            color: "#F2F6FF"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: clearPresetActionDialog.close()
                        }
                    }

                    // ---------- DELETE ----------
                    Item {
                        width: 140
                        height: 40

                        Rectangle {
                            anchors.fill: parent
                            radius: height / 2
                            color: "transparent"
                            border.color: "#3B4B63"
                            border.width: 1
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "DELETE"
                            color: "#FF5C5C"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {

                                console.log("clearPresetActionDialog.messageInt",clearPresetActionDialog.messageInt,)
                                if(clearPresetActionDialog.messageInt === 1){
                                    //SCAN
                                    mainWindows.deleteScanCardAllSlot()
                                    profilesLoaded();
                                    let cb = clearPresetActionDialog.onConfirm
                                    clearPresetActionDialog.close()
                                    if (cb) cb()
                                    console.log("messageInt === 1 SCAN")
                                }
                                else if(clearPresetActionDialog.messageInt === 2){
                                    //MEMORY
                                    configManager.deleteAllPresets()
                                    configManager.saveToFile("/var/lib/openwebrx/preset.json")
                                    radioMemList.clear()
                                    var msg = {
                                        objectName: "deletePresetAll"
                                    }
                                    mainWindows.sendmessageToWeb(JSON.stringify(msg))
                                    let cb = clearPresetActionDialog.onConfirm
                                    clearPresetActionDialog.close()
                                    if (cb) cb()
                                    console.log("messageInt === 2 MEMORY")
                                }
                                else{
                                    //EMPTY
                                    let cb = clearPresetActionDialog.onConfirm
                                    clearPresetActionDialog.close()
                                    if (cb) cb()
                                    console.log("messageInt !== ,12 EMPTY")
                                }


                            }
                        }
                    }
                }
            }
        }
    }



    MyMenuBotomBar {
        id: z
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 25
        anchors.horizontalCenter: parent.horizontalCenter
        visible: screenrotation == 0
        // toolButtonMem.onPressAndHold: {
        //     clearPresetActionDialog.open()
        // }
        // toolButtonRadio.onClicked: {
        //     stackView.pop(null)
        //     listView.currentIndex = 0
        // }
        // toolButtonMem.onClicked: {
        //     widgetView = !widgetView
        //     // listView.currentIndex = 1
        // }
        toolButtonPower.onClicked: {
            powerDialog.open()
        }
        // toolButtonSetting.onClicked: {
        //     listView.currentIndex = 3
        // }
        // toolButtonSetting.onPressAndHold: {
        //     getScreenshotTimer.start()
        // }

        // toolButtonSetting.onReleased: {

        // }
    }
    MyMenuBar {
        id: myMenuBar
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 8
        visible: screenrotation == 270
        // toolButtonMem.onPressAndHold: {
        //     clearPresetActionDialog.open()
        // }
        // toolButtonRadio.onClicked: {
        //     stackView.pop(null)
        //     listView.currentIndex = 0
        // }
        // toolButtonMem.onClicked: {
        //     widgetView = !widgetView
        //     // listView.currentIndex = 1
        // }
        toolButtonPower.onClicked: {
            powerDialog.open()
        }
        // toolButtonSetting.onClicked: {
        //     listView.currentIndex = 3
        // }
        // toolButtonSetting.onPressAndHold: {
        //     getScreenshotTimer.start()
        // }

        // toolButtonSetting.onReleased: {

        // }
    }
    Timer {
        id: getScreenshotTimer
        interval: 1000
        running: false
        repeat: false
        onTriggered: window.getScreenshot()
    }
    Dialog {
        id: powerDialog
        width: 800
        height: 300
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        modal: false
        standardButtons: Dialog.Cancel
        Material.background: "#202633"
        background:
        Rectangle {
        id: rectangle
            color: "#2C384A"
            radius: 15
        }
        // signal shutdownRequested()
        // signal rebootRequested()

        contentItem:
            RowLayout {
                width: 400
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.topMargin: 80
                anchors.bottomMargin: 100
                spacing: 20
            Layout.alignment: Qt.AlignHCenter

            ToolButton {
                id: shutdownButton
                // text: "Shutdown"
                icon.name: "power"
                font.pixelSize: 18
                Layout.fillHeight: true
                onClicked: mainWindows.shutdownRequested();
                Layout.fillWidth: true
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
                        source: "images/powerButton.png"
                        fillMode: Image.PreserveAspectFit
                    }
                }
            }

            ToolButton {
                id: rebootButton
                // text: "Reboot"
                icon.name: "refresh"
                font.pixelSize: 18
                Layout.fillHeight: true
                onClicked: mainWindows.rebootRequested();
                Layout.fillWidth: true
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
                        source: "images/restartButton.png"
                        fillMode: Image.PreserveAspectFit
                    }
                }
            }
            ToolButton {
                id: offDisplayButton
                // text: "Reboot"
                icon.name: "refresh"
                font.pixelSize: 18
                Layout.fillHeight: true
                onClicked: {
                    screenrotation == 270 ? screenOff = true : screenOff = false
                    screenrotation == 270 ? mainWindows.offScreenRequested() :  mainWindows.backlightRequested()
                    powerDialog.close()
                }
                Layout.fillWidth: true
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
                        source: screenrotation == 270 ? "images/brightness.png" : "images/backlight.png"
                        fillMode: Image.PreserveAspectFit
                    }
                }
            }
        }
        // Connections {
        //     target: powerDialog
        //     onShutdownRequested: {
        //         console.log("Shutdown requested")
        //         mainWindows.shutdownRequested();
        //     }
        //     onRebootRequested: {
        //         console.log("Reboot requested")
        //         mainWindows.rebootRequested();
        //         // Call C++ function or system command
        //     }
        // }
    }

    Rectangle {
        y: inputKey.inputPanel.x+230
        x: 263
        width: inputKey.inputPanel.width
        height: 33
        color: "#000000"
        radius: 0
        visible: Qt.inputMethod.visible && screenrotation == 270
        Text {
            id: previewText
            anchors.fill: parent
            color: "white"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignBottom
            text: Qt.inputMethod.visible &&
                  activeFocusItem !== null &&
                  typeof activeFocusItem.text === "string"
                  ? activeFocusItem.text : ""
            font.pointSize: 12
        }
    }

    Drawer {
        id: errorDialog
        y: (1280/2)-320
        width: 300
        height: 640
        implicitWidth: 0
        dim: false
        modal: true
        clip: false
        dragMargin: 0
        edge: Qt.LeftEdge
        Material.background:  "#000000"
        TextDialog {
            rotation: screenrotation
            id: errorDialogText
            width: 300
            height: 640
            title: "Error!"
            accepted.onClicked: {
                errorDialog.close()
            }
        }
    }

}

/*##^##
Designer {
    D{i:0;formeditorZoom:0.66}D{i:14}
}
##^##*/
