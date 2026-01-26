// DoaHistoryLogger.qml
import QtQuick 2.15

QtObject {
    id: logger

    // signal historyUpdated()

    property var doaHistory: []
    property var lastVfoConfig: null
    property string lastTime: ""
    property string lastDate: ""
    property string lastUptime: ""

    function saveDoa(vfoIndex, doaValue) {
        let timeStr = "[" + lastDate + " " + lastTime + "]"
        let nameStr = ""
        let freqStr = "-"
        let doaStr = "-"

        if (vfoIndex === null) {
            nameStr = "[Center Frequency]"
            if (lastVfoConfig &&
                lastVfoConfig.VFOFrequency &&
                lastVfoConfig.VFOFrequency.length > 0) {
                freqStr = Number(lastVfoConfig.VFOFrequency[0]).toFixed(3)
            }
        } else if (vfoIndex >= 0) {
            nameStr = "[VFO-" + vfoIndex + "]"
            if (lastVfoConfig &&
                lastVfoConfig.VFOFrequency &&
                lastVfoConfig.VFOFrequency.length > vfoIndex) {
                freqStr = Number(lastVfoConfig.VFOFrequency[vfoIndex]).toFixed(3)
            }
        }

        if (!isNaN(doaValue)) {
            doaStr = Number(doaValue).toFixed(3)
        }

        let line = timeStr + " " + nameStr + " [" + freqStr + "] [" + doaStr + "]"

        doaHistory.push({
            timestamp: timeStr,
            name: nameStr,
            frequency: freqStr,
            doa: doaStr,
            rawVfoIndex: vfoIndex
        })

        if (doaHistory.length > 20) {
            doaHistory.shift()
        }

        console.log("Config " + line)
        // printDoaHistory()
        // historyUpdated()
    }

    function saveTime(currentTime, currentDate, uptime) {
        lastTime = currentTime
        lastDate = currentDate
        lastUptime = uptime
    }

    function saveVfoConfig(config) {
        if (config && config.VFOFrequency && config.VFOFrequency.length > 0) {
            lastVfoConfig = config
            console.log("[VFO Config] " + JSON.stringify(config))
        } else {
            let freq = Krakenmapval.centerFrequency !== undefined
                ? Number(Krakenmapval.centerFrequency).toFixed(3)
                : "-"
            lastVfoConfig = {
                VFOFrequency: [ freq ]
            }
            console.log("[Center Frequency Config] Frequency = " + freq)
        }
    }

    function printDoaHistory() {
        console.log("=== DOA History ===")
        for (let i = 0; i < doaHistory.length; i++) {
            let item = doaHistory[i]
            console.log(
                item.timestamp + " " +
                item.name + " [" +
                item.frequency + "] [" +
                item.doa + "]"
            )
        }
        console.log("=== END ===")
    }
}
