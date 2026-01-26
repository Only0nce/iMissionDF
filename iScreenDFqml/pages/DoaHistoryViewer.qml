// // // DoaHistoryViewer.qml  (FULL FILE)
// // // LOG MODE + Max DOA log after "stable 3 seconds"
// // // ✅ USE ONLY freq/bw FROM saveRfParams(freqHz, bwHz) (Krakenmapval rfsocParameterUpdated)
// // // ✅ FIX: keep lastRfFreqHz/lastRfBwHz as "last non-zero" (never overwrite by 0)
// // // ✅ FIX: if candidate started with 0 freq/bw but later got non-zero -> update candFreqHz/candBwHz WITHOUT resetting timer
// // // ✅ NOTE: feedMaxDoaCandidate(obj) IGNORE obj.freq_hz / obj.bw_hz completely
// // // ✅ FIX: prevent stacked instances from logging (only active+visible instance processes)
// // // ✅ OPTIONAL: shared rfCache injected from MapViewer (recommended) to avoid reset issues across instances

// // import QtQuick 2.15
// // import QtQuick.Controls 2.15
// // import QtQuick.Layouts 1.15
// // import QtGraphicalEffects 1.15

// // Rectangle {
// //     id: viewer
// //     width: 440
// //     height: 380
// //     radius: 18
// //     color: "#0B1216"
// //     border.width: 1
// //     border.color: "#2A3A44"
// //     opacity: active ? 0.96 : 0.14

// //     property bool active: false
// //     property bool daqLocked: false
// //     property string displayText: ""

// //     // รับ txHistoryModel จากภายนอก
// //     property var txModel: null

// //     // ✅ inject ref from parent (MapViewer) to avoid "target: Krakenmapval not found"
// //     // MapViewer should set: doaHistoryViewer.krakenmapval = Krakenmapval
// //     property var krakenmapval: null

// //     // ✅ OPTIONAL shared cache object (recommended)
// //     // MapViewer should create rfCache QtObject { lastRfFreqHz,lastRfBwHz,save(freqHz,bwHz) } then inject:
// //     // doaHistoryViewer.rfCache = rfCache
// //     property var rfCache: null

// //     property alias logger: logger

// //     layer.enabled: true
// //     layer.effect: DropShadow {
// //         color: "#00E5FF33"
// //         radius: 22
// //         samples: 48
// //         verticalOffset: 0
// //         horizontalOffset: 0
// //     }

// //     Rectangle {
// //         anchors.fill: parent
// //         radius: viewer.radius
// //         color: "transparent"
// //         gradient: Gradient {
// //             GradientStop { position: 0.0; color: "#14212A" }
// //             GradientStop { position: 1.0; color: "#070B0E" }
// //         }
// //         opacity: 0.85
// //     }

// //     QtObject {
// //         id: logger
// //         signal historyUpdated()

// //         // ✅ instance tag (debug stacked instances)
// //         property string inst: "DH-" + Date.now() + "-" + Math.floor(Math.random()*100000)

// //         // -----------------------------
// //         // Logs
// //         // -----------------------------
// //         property var doaHistory: []
// //         property var lastVfoConfig: null
// //         property string lastTime: ""
// //         property string lastDate: ""
// //         property string lastUptime: ""

// //         // ============================================================
// //         // ✅ RF PARAMS CACHE (LAST NON-ZERO)  [FROM saveRfParams ONLY]
// //         //    - local cache (per instance) + optional shared rfCache
// //         // ============================================================
// //         property real lastRfFreqHz: 0
// //         property real lastRfBwHz: 0

// //         // ✅ update cache เฉพาะตอน "ไม่เป็น 0" (0 จะไม่ทับค่าเดิม)
// //         function saveRfParams(freqHz, bwHz) {
// //             var f = Number(freqHz)
// //             var b = Number(bwHz)

// //             // 1) update shared first (recommended source of truth)
// //             if (viewer.rfCache) {
// //                 if (typeof viewer.rfCache.save === "function") {
// //                     viewer.rfCache.save(f, b)
// //                 } else {
// //                     if (isFinite(f) && f > 0) viewer.rfCache.lastRfFreqHz = f
// //                     if (isFinite(b) && b > 0) viewer.rfCache.lastRfBwHz   = b
// //                 }
// //             }

// //             // 2) sync local from shared (so local won't stay 0)
// //             if (viewer.rfCache) {
// //                 var sf = Number(viewer.rfCache.lastRfFreqHz)
// //                 var sb = Number(viewer.rfCache.lastRfBwHz)
// //                 if (isFinite(sf) && sf > 0) lastRfFreqHz = sf
// //                 if (isFinite(sb) && sb > 0) lastRfBwHz   = sb
// //             } else {
// //                 // fallback: local only
// //                 if (isFinite(f) && f > 0) lastRfFreqHz = f
// //                 if (isFinite(b) && b > 0) lastRfBwHz   = b
// //             }

// //           /*  console.log("[" + inst + "] saveRfParams local(f=", lastRfFreqHz, " b=", lastRfBwHz, ")",
// //                         viewer.rfCache ? (" shared(f=" + viewer.rfCache.lastRfFreqHz + " b=" + viewer.rfCache.lastRfBwHz + ")") : "")*/
// //         }

// //         function _getRfFreqHzNow() {
// //             // shared first
// //             if (viewer.rfCache && isFinite(viewer.rfCache.lastRfFreqHz) && viewer.rfCache.lastRfFreqHz > 0)
// //                 return Number(viewer.rfCache.lastRfFreqHz)
// //             // local fallback
// //             if (isFinite(lastRfFreqHz) && lastRfFreqHz > 0)
// //                 return Number(lastRfFreqHz)
// //             return 0
// //         }

// //         function _getRfBwHzNow() {
// //             if (viewer.rfCache && isFinite(viewer.rfCache.lastRfBwHz) && viewer.rfCache.lastRfBwHz > 0)
// //                 return Number(viewer.rfCache.lastRfBwHz)
// //             if (isFinite(lastRfBwHz) && lastRfBwHz > 0)
// //                 return Number(lastRfBwHz)
// //             return 0
// //         }

// //         // ============================================================
// //         // ✅ Max-DOA "STABLE 3s" LOGGING (IGNORE obj.freq_hz / obj.bw_hz)
// //         // ============================================================
// //         property int  maxStableMs: 3000
// //         property real maxStableDeltaDeg: 1.0
// //         property int  maxMinIntervalMs: 300

// //         // state: candidate ที่กำลังนับ "คงเดิม"
// //         property string candKey: ""
// //         property real   candDoa: -9999
// //         property real   candConf: -1
// //         property int    candSinceMs: 0

// //         // ✅ candidate freq/bw snapshot (เริ่ม 0 ได้ แต่จะเติมทีหลังได้ โดยไม่ reset timer)
// //         property real   candFreqHz: 0
// //         property real   candBwHz: 0

// //         // state: ล่าสุดที่ log ไปแล้ว
// //         property string lastLoggedKey: ""
// //         property real   lastLoggedDoa: -9999
// //         property int    lastLoggedMs: 0

// //         function _now() { return Date.now() }

// //         function _degChanged(a, b) {
// //             if (!isFinite(a) || !isFinite(b)) return true
// //             return Math.abs(Number(a) - Number(b)) >= maxStableDeltaDeg
// //         }

// //         function _fmtDoa(v) {
// //             var x = Number(v)
// //             if (!isFinite(x)) return "-"
// //             return x.toFixed(3)
// //         }
// //         function _fmtFreqMHzFromHz(hz) {
// //             var v = Number(hz)
// //             if (!isFinite(v) || v <= 0) return "-"
// //             return (v / 1000000.0).toFixed(3) // MHz
// //         }
// //         function _fmtBwKHzFromHz(hz) {
// //             var v = Number(hz)
// //             if (!isFinite(v) || v <= 0) return "-"
// //             return (v / 1000.0).toFixed(0)    // kHz
// //         }

// //         // ✅ เรียกทุก tick ด้วย "max doa ณ ตอนนี้"
// //         // obj = { key, doa, confidence, heading, lat, lon }
// //         function feedMaxDoaCandidate(obj) {
// //             if (!obj) return

// //             // ✅ IMPORTANT: ignore hidden/inactive stacked instances
// //             // (prevents duplicated logs + "0 freq" from re-created instances)
// //             if (!viewer.visible || viewer.opacity < 0.5 || !viewer.active)
// //                 return

// //             var nowMs = _now()

// //             var key  = String(obj.key || "")
// //             var doa  = Number(obj.doa)
// //             var conf = Number(obj.confidence)

// //             if (!isFinite(doa))  doa  = 0
// //             if (!isFinite(conf)) conf = 0

// //             function pad2(v) { v = Math.floor(v); return (v < 10 ? "0" + v : "" + v) }
// //             function fmtSysDateTime(ms) {
// //                 var d = new Date(Number(ms))
// //                 var y  = d.getFullYear()
// //                 var mo = pad2(d.getMonth() + 1)
// //                 var da = pad2(d.getDate())
// //                 var hh = pad2(d.getHours())
// //                 var mm = pad2(d.getMinutes())
// //                 var ss = pad2(d.getSeconds())
// //                 return y + "-" + mo + "-" + da + " " + hh + ":" + mm + ":" + ss
// //             }

// //             // helper: เติม candFreq/candBw จาก rf cache ถ้ามี (โดยไม่ reset timer)
// //             function maybeFillCandRfSnapshot() {
// //                 if (!isFinite(candFreqHz) || candFreqHz <= 0) {
// //                     var fNow = _getRfFreqHzNow()
// //                     if (fNow > 0) candFreqHz = fNow
// //                 }
// //                 if (!isFinite(candBwHz) || candBwHz <= 0) {
// //                     var bNow = _getRfBwHzNow()
// //                     if (bNow > 0) candBwHz = bNow
// //                 }
// //             }

// //             // ถ้ายังไม่มี candidate → ตั้งเริ่มนับ
// //             if (!candKey.length) {
// //                 candKey = key
// //                 candDoa = doa
// //                 candConf = conf
// //                 candSinceMs = nowMs

// //                 // snapshot rf ณ ตอนเริ่ม (อาจเป็น 0 ได้)
// //                 candFreqHz = _getRfFreqHzNow()
// //                 candBwHz   = _getRfBwHzNow()
// //                 return
// //             }

// //             // ถ้าเปลี่ยน key หรือ doa -> reset นับใหม่
// //             var changed =
// //                     (key !== candKey) ||
// //                     _degChanged(doa, candDoa)

// //             if (changed) {
// //                 candKey = key
// //                 candDoa = doa
// //                 candConf = conf
// //                 candSinceMs = nowMs

// //                 // reset snapshot ตอนเริ่ม candidate ใหม่ (อาจ 0)
// //                 candFreqHz = _getRfFreqHzNow()
// //                 candBwHz   = _getRfBwHzNow()
// //                 return
// //             }

// //             // คงเดิมอยู่ → อัปเดต conf ล่าสุดได้ (แต่ไม่ reset เวลา)
// //             candConf = conf

// //             // ✅ ถ้าตอนเริ่ม candFreq/candBw เป็น 0 แต่ภายหลังมีค่าแล้ว -> เติมโดยไม่ reset timer
// //             maybeFillCandRfSnapshot()

// //             // รอครบ 3 วิ
// //             var stableAge = nowMs - candSinceMs
// //             if (stableAge < maxStableMs) return

// //             // กัน spam
// //             if ((nowMs - lastLoggedMs) < maxMinIntervalMs) return

// //             // กัน log ซ้ำเดิม
// //             var sameAsLast =
// //                     (candKey === lastLoggedKey) &&
// //                     !_degChanged(candDoa, lastLoggedDoa)

// //             if (sameAsLast) return

// //             // ✅ LOG
// //             lastLoggedKey = candKey
// //             lastLoggedDoa = candDoa
// //             lastLoggedMs  = nowMs

// //             var hasRemoteTime = (String(lastDate || "").length > 0) && (String(lastTime || "").length > 0)
// //             var tsRemote = hasRemoteTime ? ("[" + lastDate + " " + lastTime + "]")
// //                                          : ("[" + fmtSysDateTime(nowMs) + "]")
// //             var tsSys    = fmtSysDateTime(nowMs)

// //             var doaStr = _fmtDoa(candDoa)

// //             // ✅ NOTE: frequency ALWAYS "-" (ignore obj.freq_hz / obj.bw_hz completely)
// //             // var extra = " stable=" + Math.round(stableAge/1000) + "s"
// //             // if (candKey.length) extra += " key=" + candKey
// //             // extra += " conf=" + Number(candConf).toFixed(2)
// //             // extra += " sys=" + tsSys
// //             var extra = ""
// //             if (candKey.length) extra += " key=" + candKey
// //             extra += " conf=" + Number(candConf).toFixed(2)

// //             if (obj.heading !== undefined && isFinite(Number(obj.heading)))
// //                 extra += " hdg=" + Number(obj.heading).toFixed(1)

// //             if (obj.lat !== undefined && isFinite(Number(obj.lat)) &&
// //                 obj.lon !== undefined && isFinite(Number(obj.lon))) {
// //                 extra += " lat=" + Number(obj.lat).toFixed(6) + " lon=" + Number(obj.lon).toFixed(6)
// //             }

// //             // ✅ show frequency from candFreqHz (filled from rfCache/local, no obj.freq_hz)
// //             var freqStr = _fmtFreqMHzFromHz(candFreqHz)

// //             // (optional) ถ้าคุณอยากโชว์ BW ด้วยในช่อง frequency:
// //             var bwStr = _fmtBwKHzFromHz(candBwHz)
// //             var freqDisplay = (freqStr === "-") ? "-" : (freqStr + " MHz" + (bwStr !== "-" ? (" / " + bwStr + " kHz") : ""))

// //             // console.log("[" + inst + "] BEFORE PUSH rf(local=", lastRfFreqHz, ") rf(shared=",
// //             //            viewer.rfCache ? viewer.rfCache.lastRfFreqHz : "-", ") candFreqHz=", candFreqHz)

// //             doaHistory.push({
// //                 timestamp: tsRemote,
// //                 name: "[MAX DOA]",
// //                 frequency: freqDisplay,     // ✅ NOW shows frequency (and optional BW)
// //                 doa: doaStr + extra,
// //                 rawVfoIndex: -999
// //             })


// //             if (doaHistory.length > 80) doaHistory.shift()

// //             viewer.refresh()
// //             historyUpdated()
// //         }

// //         // (เดิม) log แบบ manual
// //         function saveDoa(vfoIndex, doaValue) {
// //             // ✅ IMPORTANT: ignore hidden/inactive stacked instances
// //             if (!viewer.visible || viewer.opacity < 0.5 || !viewer.active)
// //                 return

// //             let timeStr = "[" + lastDate + " " + lastTime + "]"
// //             let nameStr = ""
// //             let doaStr = "-"

// //             if (vfoIndex === null) {
// //                 nameStr = "[Center Frequency]"
// //             } else if (vfoIndex >= 0) {
// //                 nameStr = "[VFO-" + vfoIndex + "]"
// //             }

// //             if (!isNaN(doaValue)) doaStr = Number(doaValue).toFixed(3)

// //             doaHistory.push({
// //                 timestamp: timeStr,
// //                 name: nameStr,
// //                 frequency: "-",            // ✅ ALWAYS "-"
// //                 doa: doaStr,
// //                 rawVfoIndex: vfoIndex
// //             })
// //             if (doaHistory.length > 80) doaHistory.shift()

// //             viewer.refresh()
// //             historyUpdated()
// //         }

// //         function saveTime(currentTime, currentDate, uptime) {
// //             lastTime = currentTime
// //             lastDate = currentDate
// //             lastUptime = uptime
// //         }

// //         function saveVfoConfig(config) {
// //             lastVfoConfig = config
// //         }
// //     }

// //     Behavior on opacity {
// //         NumberAnimation { duration: 220; easing.type: Easing.InOutQuad }
// //     }

// //     Timer {
// //         id: fadeTimer
// //         interval: 2200
// //         repeat: false
// //         onTriggered: { if (!daqLocked) viewer.active = false }
// //     }

// //     function triggerFadeIn() {
// //         active = true
// //         if (!daqLocked) fadeTimer.restart()
// //     }

// //     Component.onCompleted: active = false

// //     Connections {
// //         target: logger
// //         function onHistoryUpdated() { viewer.triggerFadeIn() }
// //     }

// //     // ✅ รับค่า freq/bw จาก Krakenmapval (ผ่าน property injection)
// //     // NOTE: This is the ONLY entry path that updates RF cache.
// //     // Connections {
// //     //     target: viewer.krakenmapval
// //     //     ignoreUnknownSignals: true
// //     //     function onRfsocParameterUpdated(freqHz, bwHz) {
// //     //         logger.saveRfParams(freqHz, bwHz)
// //     //     }
// //     // }

// //     function pad2(v) { v = Math.floor(v); return (v < 10 ? "0" + v : "" + v) }
// //     function tsDateTime(ms) {
// //         if (!ms) return "-"
// //         var d = new Date(Number(ms))
// //         var y = d.getFullYear()
// //         var mo = pad2(d.getMonth() + 1)
// //         var da = pad2(d.getDate())
// //         var hh = pad2(d.getHours())
// //         var mm = pad2(d.getMinutes())
// //         var ss = pad2(d.getSeconds())
// //         return y + "-" + mo + "-" + da + " " + hh + ":" + mm + ":" + ss
// //     }

// //     // ===================== TOP BAR =====================
// //     Row {
// //         id: topBar
// //         anchors.left: parent.left
// //         anchors.right: parent.right
// //         anchors.top: parent.top
// //         anchors.margins: 14
// //         spacing: 10

// //         Column {
// //             spacing: 2
// //             Text {
// //                 text: "DoA / TX Monitor"
// //                 color: "white"
// //                 font.pixelSize: 14
// //                 font.bold: true
// //             }
// //             Text {
// //                 text: (txModel && txModel.count !== undefined)
// //                       ? ("TX points: " + txModel.count)
// //                       : "TX points: -"
// //                 color: "#A9C1CC"
// //                 font.pixelSize: 10
// //             }
// //         }

// //         Item { width: 1; height: 1; Layout.fillWidth: true }

// //         Button {
// //             id: lockFadeButton
// //             width: 34
// //             height: 34
// //             checkable: true
// //             checked: false

// //             onClicked: {
// //                 viewer.daqLocked = lockFadeButton.checked
// //                 if (viewer.daqLocked) { viewer.active = true; fadeTimer.stop() }
// //                 else fadeTimer.restart()
// //             }

// //             background: Rectangle {
// //                 radius: 12
// //                 color: lockFadeButton.checked ? "#1F6F4A" : "#0E1B22"
// //                 border.width: 1
// //                 border.color: lockFadeButton.checked ? "#2ECC71" : "#22313A"
// //             }

// //             contentItem: Image {
// //                 anchors.centerIn: parent
// //                 source: lockFadeButton.checked
// //                         ? "qrc:/iScreenDFqml/images/lock.png"
// //                         : "qrc:/iScreenDFqml/images/unlock.png"
// //                 width: 22
// //                 height: 22
// //                 fillMode: Image.PreserveAspectFit
// //             }
// //         }
// //     }

// //     // ===================== TABS =====================
// //     Row {
// //         id: tabs
// //         anchors.left: parent.left
// //         anchors.right: parent.right
// //         anchors.top: topBar.bottom
// //         anchors.leftMargin: 14
// //         anchors.rightMargin: 14
// //         anchors.topMargin: 10
// //         spacing: 8

// //         property int tab: 0 // 0=TX, 1=DOA LOG

// //         Rectangle {
// //             id: tabBg
// //             width: 140
// //             height: 34
// //             radius: 16
// //             color: "#0E1B22"
// //             border.width: 1
// //             border.color: "#22313A"

// //             Row {
// //                 anchors.fill: parent
// //                 anchors.margins: 4
// //                 spacing: 4

// //                 Rectangle {
// //                     width: 64
// //                     height: parent.height
// //                     radius: 14
// //                     color: tabs.tab === 0 ? "#FFB300" : "transparent"
// //                     Text {
// //                         anchors.centerIn: parent
// //                         text: "TX"
// //                         color: tabs.tab === 0 ? "#0B1216" : "#A9C1CC"
// //                         font.bold: true
// //                         font.pixelSize: 12
// //                     }
// //                     MouseArea { anchors.fill: parent; onClicked: tabs.tab = 0 }
// //                 }

// //                 Rectangle {
// //                     width: 64
// //                     height: parent.height
// //                     radius: 14
// //                     color: tabs.tab === 1 ? "#00FFAA" : "transparent"
// //                     Text {
// //                         anchors.centerIn: parent
// //                         text: "DOA"
// //                         color: tabs.tab === 1 ? "#0B1216" : "#A9C1CC"
// //                         font.bold: true
// //                         font.pixelSize: 12
// //                     }
// //                     MouseArea { anchors.fill: parent; onClicked: tabs.tab = 1 }
// //                 }
// //             }
// //         }

// //         Item { width: 1; height: 1; Layout.fillWidth: true }

// //         Text {
// //             anchors.verticalCenter: tabBg.verticalCenter
// //             text: tabs.tab === 0 ? "Latest point highlighted" : "Max DOA logs only after stable 3s"
// //             color: "#6F8C98"
// //             font.pixelSize: 10
// //         }
// //     }

// //     // ===================== CONTENT =====================
// //     Item {
// //         id: content
// //         anchors.left: parent.left
// //         anchors.right: parent.right
// //         anchors.top: tabs.bottom
// //         anchors.bottom: parent.bottom
// //         anchors.margins: 14

// //         // ---------- TX PANEL ----------
// //         Item {
// //             anchors.fill: parent
// //             visible: tabs.tab === 0

// //             Rectangle {
// //                 anchors.fill: parent
// //                 radius: 14
// //                 color: "#0E1B22"
// //                 border.width: 1
// //                 border.color: "#22313A"
// //                 opacity: 0.95
// //             }

// //             Item {
// //                 anchors.fill: parent
// //                 visible: !txModel || (txModel.count !== undefined && txModel.count === 0)

// //                 Column {
// //                     anchors.centerIn: parent
// //                     spacing: 6

// //                     Text {
// //                         text: "No TX history yet"
// //                         color: "white"
// //                         font.pixelSize: 13
// //                         font.bold: true
// //                         horizontalAlignment: Text.AlignHCenter
// //                     }

// //                     Text {
// //                         text: "Waiting for 2+ DOA intersection..."
// //                         color: "#A9C1CC"
// //                         font.pixelSize: 11
// //                         horizontalAlignment: Text.AlignHCenter
// //                     }
// //                 }
// //             }

// //             ListView {
// //                 id: txList
// //                 anchors.fill: parent
// //                 anchors.margins: 10
// //                 clip: true
// //                 spacing: 8
// //                 model: txModel
// //                 visible: txModel && (txModel.count === undefined || txModel.count > 0)

// //                 boundsBehavior: Flickable.StopAtBounds
// //                 flickDeceleration: 1800
// //                 maximumFlickVelocity: 2500

// //                 delegate: Rectangle {
// //                     width: txList.width
// //                     height: 70
// //                     radius: 14
// //                     color: (index === 0) ? "#14212A" : "#0B1216"
// //                     border.width: 1
// //                     border.color: (index === 0) ? "#FFB300" : "#22313A"
// //                     opacity: 0.97

// //                     Rectangle {
// //                         x: 10; y: 16
// //                         width: 38; height: 38
// //                         radius: 14
// //                         color: (index === 0) ? "#FFB300" : "#0E1B22"
// //                         border.width: 1
// //                         border.color: (index === 0) ? "#FFB300" : "#22313A"

// //                         Text {
// //                             anchors.centerIn: parent
// //                             text: "#" + (index + 1)
// //                             color: (index === 0) ? "#0B1216" : "#A9C1CC"
// //                             font.bold: true
// //                             font.pixelSize: 11
// //                         }
// //                     }

// //                     Column {
// //                         anchors.left: parent.left
// //                         anchors.leftMargin: 58
// //                         anchors.verticalCenter: parent.verticalCenter
// //                         spacing: 4

// //                         Text {
// //                             text: "lat " + Number(model.lat).toFixed(6) + "   lon " + Number(model.lon).toFixed(6)
// //                             color: "white"
// //                             font.pixelSize: 12
// //                             font.bold: true
// //                             font.family: "Monospace"
// //                         }

// //                         Row {
// //                             spacing: 10

// //                             Text {
// //                                 text: "rms " + Math.round(Number(model.rms || 0)) + " m"
// //                                 color: "#FFB300"
// //                                 font.pixelSize: 10
// //                                 font.bold: true
// //                             }

// //                             Text {
// //                                 text: "time " + viewer.tsDateTime(model.updatedMs)
// //                                 color: "#A9C1CC"
// //                                 font.pixelSize: 10
// //                                 font.family: "Monospace"
// //                             }
// //                         }
// //                     }

// //                     Rectangle {
// //                         anchors.right: parent.right
// //                         anchors.verticalCenter: parent.verticalCenter
// //                         anchors.rightMargin: 10
// //                         width: 6
// //                         height: parent.height - 18
// //                         radius: 3
// //                         color: (index === 0) ? "#FFB300" : "#22313A"
// //                         opacity: 0.9
// //                     }
// //                 }

// //                 ScrollBar.vertical: ScrollBar {
// //                     active: true
// //                     policy: ScrollBar.AsNeeded
// //                 }
// //             }
// //         }

// //         // ---------- DOA LOG PANEL ----------
// //         Item {
// //             anchors.fill: parent
// //             visible: tabs.tab === 1

// //             Rectangle {
// //                 anchors.fill: parent
// //                 radius: 14
// //                 color: "#0E1B22"
// //                 border.width: 1
// //                 border.color: "#22313A"
// //                 opacity: 0.95
// //             }

// //             ScrollView {
// //                 anchors.fill: parent
// //                 anchors.margins: 10
// //                 clip: true

// //                 TextArea {
// //                     text: viewer.displayText
// //                     wrapMode: Text.Wrap
// //                     readOnly: true
// //                     color: "#00FFAA"
// //                     font.pixelSize: 12
// //                     font.family: "Monospace"
// //                     background: null
// //                     padding: 10
// //                     implicitHeight: contentHeight
// //                 }
// //             }
// //         }
// //     }

// //     MouseArea {
// //         anchors.fill: parent
// //         z: 10000
// //         visible: !viewer.active
// //         onPressed: viewer.triggerFadeIn()
// //         propagateComposedEvents: true
// //     }

// //     // function refresh() {
// //     //     let lines = []

// //     //     for (let i = logger.doaHistory.length - 1; i >= 0; i--) {
// //     //         let item = logger.doaHistory[i]

// //     //         // main line
// //     //         lines.push(
// //     //             item.timestamp + " " +
// //     //             item.name + " [" +
// //     //             item.frequency + "]"
// //     //         )

// //     //         // second line (DOA + extra)
// //     //         lines.push("  " + item.doa)

// //     //         // separator (not after last)
// //     //         if (i !== 0) {
// //     //             lines.push("------------------------------------------------------------")
// //     //         }
// //     //     }

// //     //     displayText = lines.join("\n")
// //     // }
// //     function refresh() {
// //         let lines = []

// //         for (let i = logger.doaHistory.length - 1; i >= 0; i--) {
// //             let item = logger.doaHistory[i]

// //             // แยก doa หลัก กับ extra
// //             // item.doa รูปแบบ: "210.123 key=RX1 conf=0.87 ..."
// //             let doaParts = String(item.doa).split(" ")
// //             let doaValue = doaParts.shift()          // "210.123"
// //             let extra    = doaParts.join(" ")        // ที่เหลือทั้งหมด

// //             // ---------- main line ----------
// //             lines.push(
// //                 item.timestamp +
// //                 item.name +
// //                 "[" + doaValue + "] " +
// //                 "[" + item.frequency + "]"
// //             )

// //             // ---------- second line (extra only) ----------
// //             if (extra.length > 0) {
// //                 lines.push(" " + extra)
// //             }

// //             // ---------- separator ----------
// //             if (i !== 0) {
// //                 lines.push("------------------------------------------------------------")
// //             }
// //         }

// //         displayText = lines.join("\n")
// //     }
// // }
// // DoaHistoryViewer.qml  (FULL FILE)
// // LOG MODE + Max DOA log after "stable 3 seconds"
// // ✅ USE ONLY freq/bw FROM saveRfParams(freqHz, bwHz) (Krakenmapval rfsocParameterUpdated)
// // ✅ FIX: keep lastRfFreqHz/lastRfBwHz as "last non-zero" (never overwrite by 0)
// // ✅ FIX: if candidate started with 0 freq/bw but later got non-zero -> update candFreqHz/candBwHz WITHOUT resetting timer
// // ✅ NOTE: feedMaxDoaCandidate(obj) IGNORE obj.freq_hz / obj.bw_hz completely
// // ✅ FIX: prevent stacked instances from logging (only active instance should log)
// // ✅ NEW: DOA LOG PANEL can scroll while updates come in (freeze displayText while user scrolls)
// // ✅ IMPORTANT FIX: logging must NOT depend on viewer.active / opacity anymore (otherwise it stops logging)

// import QtQuick 2.15
// import QtQuick.Controls 2.15
// import QtQuick.Layouts 1.15
// import QtGraphicalEffects 1.15

// Rectangle {
//     id: viewer
//     width: 440
//     height: 380
//     radius: 18
//     color: "#0B1216"
//     border.width: 1
//     border.color: "#2A3A44"
//     opacity: active ? 0.96 : 0.14

//     property bool active: false
//     property bool daqLocked: false
//     property string displayText: ""

//     // ✅ Only ONE instance should log
//     // MapViewer (recommended): set doaHistoryViewer.loggingEnabled = true for the active overlay only,
//     // and set false for any stacked/hidden instances.
//     property bool loggingEnabled: true

//     // รับ txHistoryModel จากภายนอก
//     property var txModel: null

//     // ✅ inject ref from parent (MapViewer) to avoid "target: Krakenmapval not found"
//     property var krakenmapval: null

//     // ✅ OPTIONAL shared cache object (recommended)
//     property var rfCache: null

//     property alias logger: logger

//     // ================================
//     // ✅ Freeze DOA text while user scrolls
//     // ================================
//     property bool freezeDoaText: false
//     property string pendingDisplayText: ""

//     Timer {
//         id: unfreezeDoaTimer
//         interval: 450
//         repeat: false
//         onTriggered: {
//             viewer.freezeDoaText = false
//             if (viewer.pendingDisplayText.length > 0) {
//                 viewer.displayText = viewer.pendingDisplayText
//                 viewer.pendingDisplayText = ""
//             }
//         }
//     }

//     function freezeDoaTextBrief() {
//         viewer.freezeDoaText = true
//         unfreezeDoaTimer.restart()
//     }

//     layer.enabled: true
//     layer.effect: DropShadow {
//         color: "#00E5FF33"
//         radius: 22
//         samples: 48
//         verticalOffset: 0
//         horizontalOffset: 0
//     }

//     Rectangle {
//         anchors.fill: parent
//         radius: viewer.radius
//         color: "transparent"
//         gradient: Gradient {
//             GradientStop { position: 0.0; color: "#14212A" }
//             GradientStop { position: 1.0; color: "#070B0E" }
//         }
//         opacity: 0.85
//     }

//     QtObject {
//         id: logger
//         signal historyUpdated()

//         // ✅ instance tag (debug stacked instances)
//         property string inst: "DH-" + Date.now() + "-" + Math.floor(Math.random()*100000)

//         // -----------------------------
//         // Logs
//         // -----------------------------
//         property var doaHistory: []
//         property var lastVfoConfig: null
//         property string lastTime: ""
//         property string lastDate: ""
//         property string lastUptime: ""

//         // ============================================================
//         // ✅ RF PARAMS CACHE (LAST NON-ZERO)  [FROM saveRfParams ONLY]
//         //    - local cache (per instance) + optional shared rfCache
//         // ============================================================
//         property real lastRfFreqHz: 0
//         property real lastRfBwHz: 0

//         // ✅ update cache เฉพาะตอน "ไม่เป็น 0" (0 จะไม่ทับค่าเดิม)
//         function saveRfParams(freqHz, bwHz) {
//             var f = Number(freqHz)
//             var b = Number(bwHz)

//             // 1) update shared first (recommended source of truth)
//             if (viewer.rfCache) {
//                 if (typeof viewer.rfCache.save === "function") {
//                     viewer.rfCache.save(f, b)
//                 } else {
//                     if (isFinite(f) && f > 0) viewer.rfCache.lastRfFreqHz = f
//                     if (isFinite(b) && b > 0) viewer.rfCache.lastRfBwHz   = b
//                 }
//             }

//             // 2) sync local from shared (so local won't stay 0)
//             if (viewer.rfCache) {
//                 var sf = Number(viewer.rfCache.lastRfFreqHz)
//                 var sb = Number(viewer.rfCache.lastRfBwHz)
//                 if (isFinite(sf) && sf > 0) lastRfFreqHz = sf
//                 if (isFinite(sb) && sb > 0) lastRfBwHz   = sb
//             } else {
//                 // fallback: local only
//                 if (isFinite(f) && f > 0) lastRfFreqHz = f
//                 if (isFinite(b) && b > 0) lastRfBwHz   = b
//             }
//         }

//         function _getRfFreqHzNow() {
//             if (viewer.rfCache && isFinite(viewer.rfCache.lastRfFreqHz) && viewer.rfCache.lastRfFreqHz > 0)
//                 return Number(viewer.rfCache.lastRfFreqHz)
//             if (isFinite(lastRfFreqHz) && lastRfFreqHz > 0)
//                 return Number(lastRfFreqHz)
//             return 0
//         }

//         function _getRfBwHzNow() {
//             if (viewer.rfCache && isFinite(viewer.rfCache.lastRfBwHz) && viewer.rfCache.lastRfBwHz > 0)
//                 return Number(viewer.rfCache.lastRfBwHz)
//             if (isFinite(lastRfBwHz) && lastRfBwHz > 0)
//                 return Number(lastRfBwHz)
//             return 0
//         }

//         // ============================================================
//         // ✅ Max-DOA "STABLE 3s" LOGGING (IGNORE obj.freq_hz / obj.bw_hz)
//         // ============================================================
//         property int  maxStableMs: 3000
//         property real maxStableDeltaDeg: 1.0
//         property int  maxMinIntervalMs: 300

//         // state: candidate ที่กำลังนับ "คงเดิม"
//         property string candKey: ""
//         property real   candDoa: -9999
//         property real   candConf: -1
//         property int    candSinceMs: 0

//         // ✅ candidate freq/bw snapshot (เริ่ม 0 ได้ แต่จะเติมทีหลังได้ โดยไม่ reset timer)
//         property real   candFreqHz: 0
//         property real   candBwHz: 0

//         // state: ล่าสุดที่ log ไปแล้ว
//         property string lastLoggedKey: ""
//         property real   lastLoggedDoa: -9999
//         property int    lastLoggedMs: 0

//         function _now() { return Date.now() }

//         function _degChanged(a, b) {
//             if (!isFinite(a) || !isFinite(b)) return true
//             return Math.abs(Number(a) - Number(b)) >= maxStableDeltaDeg
//         }

//         function _fmtDoa(v) {
//             var x = Number(v)
//             if (!isFinite(x)) return "-"
//             return x.toFixed(3)
//         }
//         function _fmtFreqMHzFromHz(hz) {
//             var v = Number(hz)
//             if (!isFinite(v) || v <= 0) return "-"
//             return (v / 1000000.0).toFixed(3) // MHz
//         }
//         function _fmtBwKHzFromHz(hz) {
//             var v = Number(hz)
//             if (!isFinite(v) || v <= 0) return "-"
//             return (v / 1000.0).toFixed(0)    // kHz
//         }

//         // ✅ เรียกทุก tick ด้วย "max doa ณ ตอนนี้"
//         // obj = { key, doa, confidence, heading, lat, lon }
//         function feedMaxDoaCandidate(obj) {
//             if (!obj) return

//             // ✅ IMPORTANT: logging must not depend on viewer.active / opacity
//             // Only block when truly hidden OR disabled (to avoid stacked instances)
//             if (!viewer.visible || !viewer.loggingEnabled)
//                 return

//             var nowMs = _now()

//             var key  = String(obj.key || "")
//             var doa  = Number(obj.doa)
//             var conf = Number(obj.confidence)

//             if (!isFinite(doa))  doa  = 0
//             if (!isFinite(conf)) conf = 0

//             function pad2(v) { v = Math.floor(v); return (v < 10 ? "0" + v : "" + v) }
//             function fmtSysDateTime(ms) {
//                 var d = new Date(Number(ms))
//                 var y  = d.getFullYear()
//                 var mo = pad2(d.getMonth() + 1)
//                 var da = pad2(d.getDate())
//                 var hh = pad2(d.getHours())
//                 var mm = pad2(d.getMinutes())
//                 var ss = pad2(d.getSeconds())
//                 return y + "-" + mo + "-" + da + " " + hh + ":" + mm + ":" + ss
//             }

//             // helper: เติม candFreq/candBw จาก rf cache ถ้ามี (โดยไม่ reset timer)
//             function maybeFillCandRfSnapshot() {
//                 if (!isFinite(candFreqHz) || candFreqHz <= 0) {
//                     var fNow = _getRfFreqHzNow()
//                     if (fNow > 0) candFreqHz = fNow
//                 }
//                 if (!isFinite(candBwHz) || candBwHz <= 0) {
//                     var bNow = _getRfBwHzNow()
//                     if (bNow > 0) candBwHz = bNow
//                 }
//             }

//             // ถ้ายังไม่มี candidate → ตั้งเริ่มนับ
//             if (!candKey.length) {
//                 candKey = key
//                 candDoa = doa
//                 candConf = conf
//                 candSinceMs = nowMs

//                 // snapshot rf ณ ตอนเริ่ม (อาจเป็น 0 ได้)
//                 candFreqHz = _getRfFreqHzNow()
//                 candBwHz   = _getRfBwHzNow()
//                 return
//             }

//             // ถ้าเปลี่ยน key หรือ doa -> reset นับใหม่
//             var changed =
//                     (key !== candKey) ||
//                     _degChanged(doa, candDoa)

//             if (changed) {
//                 candKey = key
//                 candDoa = doa
//                 candConf = conf
//                 candSinceMs = nowMs

//                 // reset snapshot ตอนเริ่ม candidate ใหม่ (อาจ 0)
//                 candFreqHz = _getRfFreqHzNow()
//                 candBwHz   = _getRfBwHzNow()
//                 return
//             }

//             // คงเดิมอยู่ → อัปเดต conf ล่าสุดได้ (แต่ไม่ reset เวลา)
//             candConf = conf

//             // ✅ ถ้าตอนเริ่ม candFreq/candBw เป็น 0 แต่ภายหลังมีค่าแล้ว -> เติมโดยไม่ reset timer
//             maybeFillCandRfSnapshot()

//             // รอครบ 3 วิ
//             var stableAge = nowMs - candSinceMs
//             if (stableAge < maxStableMs) return

//             // กัน spam
//             if ((nowMs - lastLoggedMs) < maxMinIntervalMs) return

//             // กัน log ซ้ำเดิม
//             var sameAsLast =
//                     (candKey === lastLoggedKey) &&
//                     !_degChanged(candDoa, lastLoggedDoa)

//             if (sameAsLast) return

//             // ✅ LOG
//             lastLoggedKey = candKey
//             lastLoggedDoa = candDoa
//             lastLoggedMs  = nowMs

//             var hasRemoteTime = (String(lastDate || "").length > 0) && (String(lastTime || "").length > 0)
//             var tsRemote = hasRemoteTime ? ("[" + lastDate + " " + lastTime + "]")
//                                          : ("[" + fmtSysDateTime(nowMs) + "]")
//             var doaStr = _fmtDoa(candDoa)

//             var extra = ""
//             if (candKey.length) extra += " key=" + candKey
//             extra += " conf=" + Number(candConf).toFixed(2)

//             if (obj.heading !== undefined && isFinite(Number(obj.heading)))
//                 extra += " hdg=" + Number(obj.heading).toFixed(1)

//             if (obj.lat !== undefined && isFinite(Number(obj.lat)) &&
//                 obj.lon !== undefined && isFinite(Number(obj.lon))) {
//                 extra += " lat=" + Number(obj.lat).toFixed(6) + " lon=" + Number(obj.lon).toFixed(6)
//             }

//             // ✅ show frequency from candFreqHz (filled from rfCache/local, no obj.freq_hz)
//             var freqStr = _fmtFreqMHzFromHz(candFreqHz)
//             var bwStr = _fmtBwKHzFromHz(candBwHz)
//             var freqDisplay = (freqStr === "-") ? "-" : (freqStr + " MHz" + (bwStr !== "-" ? (" / " + bwStr + " kHz") : ""))

//             doaHistory.push({
//                 timestamp: tsRemote,
//                 name: "[MAX DOA]",
//                 frequency: freqDisplay,
//                 doa: doaStr + extra,
//                 rawVfoIndex: -999
//             })

//             if (doaHistory.length > 80) doaHistory.shift()

//             viewer.refresh()
//             historyUpdated()
//         }

//         // (เดิม) log แบบ manual
//         function saveDoa(vfoIndex, doaValue) {
//             // ✅ IMPORTANT: logging must not depend on viewer.active / opacity
//             if (!viewer.visible || !viewer.loggingEnabled)
//                 return

//             let timeStr = "[" + lastDate + " " + lastTime + "]"
//             let nameStr = ""
//             let doaStr = "-"

//             if (vfoIndex === null) {
//                 nameStr = "[Center Frequency]"
//             } else if (vfoIndex >= 0) {
//                 nameStr = "[VFO-" + vfoIndex + "]"
//             }

//             if (!isNaN(doaValue)) doaStr = Number(doaValue).toFixed(3)

//             doaHistory.push({
//                 timestamp: timeStr,
//                 name: nameStr,
//                 frequency: "-",
//                 doa: doaStr,
//                 rawVfoIndex: vfoIndex
//             })
//             if (doaHistory.length > 80) doaHistory.shift()

//             viewer.refresh()
//             historyUpdated()
//         }

//         function saveTime(currentTime, currentDate, uptime) {
//             lastTime = currentTime
//             lastDate = currentDate
//             lastUptime = uptime
//         }

//         function saveVfoConfig(config) {
//             lastVfoConfig = config
//         }
//     }

//     Behavior on opacity {
//         NumberAnimation { duration: 220; easing.type: Easing.InOutQuad }
//     }

//     Timer {
//         id: fadeTimer
//         interval: 2200
//         repeat: false
//         onTriggered: { if (!daqLocked) viewer.active = false }
//     }

//     function triggerFadeIn() {
//         active = true
//         if (!daqLocked) fadeTimer.restart()
//     }

//     Component.onCompleted: active = false

//     Connections {
//         target: logger
//         function onHistoryUpdated() { viewer.triggerFadeIn() }
//     }

//     // ✅ NOTE: Connect rfsocParameterUpdated outside or enable here if you want
//     // Connections {
//     //     target: viewer.krakenmapval
//     //     ignoreUnknownSignals: true
//     //     function onRfsocParameterUpdated(freqHz, bwHz) {
//     //         logger.saveRfParams(freqHz, bwHz)
//     //     }
//     // }

//     function pad2(v) { v = Math.floor(v); return (v < 10 ? "0" + v : "" + v) }
//     function tsDateTime(ms) {
//         if (!ms) return "-"
//         var d = new Date(Number(ms))
//         var y = d.getFullYear()
//         var mo = pad2(d.getMonth() + 1)
//         var da = pad2(d.getDate())
//         var hh = pad2(d.getHours())
//         var mm = pad2(d.getMinutes())
//         var ss = pad2(d.getSeconds())
//         return y + "-" + mo + "-" + da + " " + hh + ":" + mm + ":" + ss
//     }

//     // ===================== TOP BAR =====================
//     Row {
//         id: topBar
//         anchors.left: parent.left
//         anchors.right: parent.right
//         anchors.top: parent.top
//         anchors.margins: 14
//         spacing: 10

//         Column {
//             spacing: 2
//             Text {
//                 text: "DoA / TX Monitor"
//                 color: "white"
//                 font.pixelSize: 14
//                 font.bold: true
//             }
//             Text {
//                 text: (txModel && txModel.count !== undefined)
//                       ? ("TX points: " + txModel.count)
//                       : "TX points: -"
//                 color: "#A9C1CC"
//                 font.pixelSize: 10
//             }
//         }

//         Item { width: 1; height: 1; Layout.fillWidth: true }

//         Button {
//             id: lockFadeButton
//             width: 34
//             height: 34
//             checkable: true
//             checked: false

//             onClicked: {
//                 viewer.daqLocked = lockFadeButton.checked
//                 if (viewer.daqLocked) { viewer.active = true; fadeTimer.stop() }
//                 else fadeTimer.restart()
//             }

//             background: Rectangle {
//                 radius: 12
//                 color: lockFadeButton.checked ? "#1F6F4A" : "#0E1B22"
//                 border.width: 1
//                 border.color: lockFadeButton.checked ? "#2ECC71" : "#22313A"
//             }

//             contentItem: Image {
//                 anchors.centerIn: parent
//                 source: lockFadeButton.checked
//                         ? "qrc:/iScreenDFqml/images/lock.png"
//                         : "qrc:/iScreenDFqml/images/unlock.png"
//                 width: 22
//                 height: 22
//                 fillMode: Image.PreserveAspectFit
//             }
//         }
//     }

//     // ===================== TABS =====================
//     Row {
//         id: tabs
//         anchors.left: parent.left
//         anchors.right: parent.right
//         anchors.top: topBar.bottom
//         anchors.leftMargin: 14
//         anchors.rightMargin: 14
//         anchors.topMargin: 10
//         spacing: 8

//         property int tab: 0 // 0=TX, 1=DOA LOG

//         Rectangle {
//             id: tabBg
//             width: 140
//             height: 34
//             radius: 16
//             color: "#0E1B22"
//             border.width: 1
//             border.color: "#22313A"

//             Row {
//                 anchors.fill: parent
//                 anchors.margins: 4
//                 spacing: 4

//                 Rectangle {
//                     width: 64
//                     height: parent.height
//                     radius: 14
//                     color: tabs.tab === 0 ? "#FFB300" : "transparent"
//                     Text {
//                         anchors.centerIn: parent
//                         text: "TX"
//                         color: tabs.tab === 0 ? "#0B1216" : "#A9C1CC"
//                         font.bold: true
//                         font.pixelSize: 12
//                     }
//                     MouseArea { anchors.fill: parent; onClicked: tabs.tab = 0 }
//                 }

//                 Rectangle {
//                     width: 64
//                     height: parent.height
//                     radius: 14
//                     color: tabs.tab === 1 ? "#00FFAA" : "transparent"
//                     Text {
//                         anchors.centerIn: parent
//                         text: "DOA"
//                         color: tabs.tab === 1 ? "#0B1216" : "#A9C1CC"
//                         font.bold: true
//                         font.pixelSize: 12
//                     }
//                     MouseArea { anchors.fill: parent; onClicked: tabs.tab = 1 }
//                 }
//             }
//         }

//         Item { width: 1; height: 1; Layout.fillWidth: true }

//         Text {
//             anchors.verticalCenter: tabBg.verticalCenter
//             text: tabs.tab === 0 ? "Latest point highlighted" : "Max DOA logs only after stable 3s"
//             color: "#6F8C98"
//             font.pixelSize: 10
//         }
//     }

//     // ===================== CONTENT =====================
//     Item {
//         id: content
//         anchors.left: parent.left
//         anchors.right: parent.right
//         anchors.top: tabs.bottom
//         anchors.bottom: parent.bottom
//         anchors.margins: 14

//         // ---------- TX PANEL ----------
//         Item {
//             anchors.fill: parent
//             visible: tabs.tab === 0

//             Rectangle {
//                 anchors.fill: parent
//                 radius: 14
//                 color: "#0E1B22"
//                 border.width: 1
//                 border.color: "#22313A"
//                 opacity: 0.95
//             }

//             Item {
//                 anchors.fill: parent
//                 visible: !txModel || (txModel.count !== undefined && txModel.count === 0)

//                 Column {
//                     anchors.centerIn: parent
//                     spacing: 6

//                     Text {
//                         text: "No TX history yet"
//                         color: "white"
//                         font.pixelSize: 13
//                         font.bold: true
//                         horizontalAlignment: Text.AlignHCenter
//                     }

//                     Text {
//                         text: "Waiting for 2+ DOA intersection..."
//                         color: "#A9C1CC"
//                         font.pixelSize: 11
//                         horizontalAlignment: Text.AlignHCenter
//                     }
//                 }
//             }

//             ListView {
//                 id: txList
//                 anchors.fill: parent
//                 anchors.margins: 10
//                 clip: true
//                 spacing: 8
//                 model: txModel
//                 visible: txModel && (txModel.count === undefined || txModel.count > 0)

//                 boundsBehavior: Flickable.StopAtBounds
//                 flickDeceleration: 1800
//                 maximumFlickVelocity: 2500

//                 delegate: Rectangle {
//                     width: txList.width
//                     height: 70
//                     radius: 14
//                     color: (index === 0) ? "#14212A" : "#0B1216"
//                     border.width: 1
//                     border.color: (index === 0) ? "#FFB300" : "#22313A"
//                     opacity: 0.97

//                     Rectangle {
//                         x: 10; y: 16
//                         width: 38; height: 38
//                         radius: 14
//                         color: (index === 0) ? "#FFB300" : "#0E1B22"
//                         border.width: 1
//                         border.color: (index === 0) ? "#FFB300" : "#22313A"

//                         Text {
//                             anchors.centerIn: parent
//                             text: "#" + (index + 1)
//                             color: (index === 0) ? "#0B1216" : "#A9C1CC"
//                             font.bold: true
//                             font.pixelSize: 11
//                         }
//                     }

//                     Column {
//                         anchors.left: parent.left
//                         anchors.leftMargin: 58
//                         anchors.verticalCenter: parent.verticalCenter
//                         spacing: 4

//                         Text {
//                             text: "lat " + Number(model.lat).toFixed(6) + "   lon " + Number(model.lon).toFixed(6)
//                             color: "white"
//                             font.pixelSize: 12
//                             font.bold: true
//                             font.family: "Monospace"
//                         }

//                         Row {
//                             spacing: 10

//                             Text {
//                                 text: "rms " + Math.round(Number(model.rms || 0)) + " m"
//                                 color: "#FFB300"
//                                 font.pixelSize: 10
//                                 font.bold: true
//                             }

//                             Text {
//                                 text: "time " + viewer.tsDateTime(model.updatedMs)
//                                 color: "#A9C1CC"
//                                 font.pixelSize: 10
//                                 font.family: "Monospace"
//                             }
//                         }
//                     }

//                     Rectangle {
//                         anchors.right: parent.right
//                         anchors.verticalCenter: parent.verticalCenter
//                         anchors.rightMargin: 10
//                         width: 6
//                         height: parent.height - 18
//                         radius: 3
//                         color: (index === 0) ? "#FFB300" : "#22313A"
//                         opacity: 0.9
//                     }
//                 }

//                 ScrollBar.vertical: ScrollBar {
//                     active: true
//                     policy: ScrollBar.AsNeeded
//                 }
//             }
//         }

//         // ---------- DOA LOG PANEL ----------
//         Item {
//             id: doaPanel
//             anchors.fill: parent
//             visible: tabs.tab === 1

//             Rectangle {
//                 anchors.fill: parent
//                 radius: 14
//                 color: "#0E1B22"
//                 border.width: 1
//                 border.color: "#22313A"
//                 opacity: 0.95
//             }

//             ScrollView {
//                 id: doaScrollView
//                 anchors.fill: parent
//                 anchors.margins: 10
//                 clip: true

//                 TextArea {
//                     id: doaTextArea
//                     text: viewer.displayText
//                     wrapMode: Text.Wrap
//                     readOnly: true
//                     color: "#00FFAA"
//                     font.pixelSize: 12
//                     font.family: "Monospace"
//                     background: null
//                     padding: 10
//                     implicitHeight: contentHeight
//                 }

//                 // ✅ Freeze displayText while user scrolls/drag/wheel
//                 MouseArea {
//                     anchors.fill: parent
//                     acceptedButtons: Qt.NoButton
//                     propagateComposedEvents: true
//                     hoverEnabled: true

//                     onWheel: viewer.freezeDoaTextBrief()
//                     onPressed: viewer.freezeDoaTextBrief()
//                     onPositionChanged: viewer.freezeDoaTextBrief()
//                     onReleased: viewer.freezeDoaTextBrief()
//                     onCanceled: viewer.freezeDoaTextBrief()
//                 }
//             }
//         }
//     }

//     MouseArea {
//         anchors.fill: parent
//         z: 10000
//         visible: !viewer.active
//         onPressed: viewer.triggerFadeIn()
//         propagateComposedEvents: true
//     }

//     // ============================
//     // ✅ Refresh (with DOA scroll freeze)
//     // ============================
//     function refresh() {
//         let lines = []

//         for (let i = logger.doaHistory.length - 1; i >= 0; i--) {
//             let item = logger.doaHistory[i]

//             // แยก doa หลัก กับ extra
//             let doaParts = String(item.doa).split(" ")
//             let doaValue = doaParts.shift()
//             let extra    = doaParts.join(" ")

//             // main line
//             lines.push(
//                 item.timestamp +
//                 item.name +
//                 "[" + doaValue + "] " +
//                 "[" + item.frequency + "]"
//             )

//             // second line (extra only)
//             if (extra.length > 0) {
//                 lines.push(" " + extra)
//             }

//             if (i !== 0) {
//                 lines.push("------------------------------------------------------------")
//             }
//         }

//         var newText = lines.join("\n")

//         // ✅ If user is scrolling DOA tab, don't overwrite TextArea now
//         if (tabs.tab === 1 && viewer.freezeDoaText) {
//             viewer.pendingDisplayText = newText
//             return
//         }

//         viewer.displayText = newText
//     }
// }
// DoaHistoryViewer.qml (FULL FILE)
// ✅ FIX: TX emit no longer duplicates
//    - Coalesce ALL txModel signals into ONE emit per event-loop (Qt.callLater)
//    - Hard de-dupe by modelKey (lat/lon/rms/updatedMs) => same TX point never re-sent
// ✅ FIX: use viewer.krakenmapval (injected) ONLY (no global Krakenmapval)
// ✅ TX PANEL + coord toggle (lat/lon vs MGRS) persisted via Settings
// ✅ NOTE: MapViewer should inject:
//    doaHistoryViewer.txModel = txHistoryModel
//    doaHistoryViewer.krakenmapval = Krakenmapval
//    doaHistoryViewer.rfCache = rfCache
//    doaHistoryViewer.loggingEnabled = true for the active overlay only
import QtQuick 2.15
import Qt.labs.settings 1.1
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15

Rectangle {
    id: viewer
    width: 530
    height: 440
    radius: 18
    color: "#0B1216"
    border.width: 1
    border.color: "#2A3A44"
    opacity: active ? 0.72 : 0.14

    property bool active: false
    property bool daqLocked: false

    // ✅ only ONE instance should log/send (set from MapViewer for the active overlay)
    property bool loggingEnabled: true

    // external models/refs
    property var txModel: null
    property var krakenmapval: null
    property var rfCache: null

    property alias logger: logger

    // ✅ DOA log model for UI (append-only)
    ListModel { id: doaLogModel }
    property int maxLogItems: 100

    // ============================================================
    // TX Coordinate display mode (persist)
    // ============================================================
    // 0 = lat/lon, 1 = MGRS
    property int txCoordMode: 1

    Settings {
        id: viewSettings
        category: "DoaHistoryViewer"
        property int txCoordMode: 1
        onTxCoordModeChanged: viewer.txCoordMode = txCoordMode
    }

    // ============================================================
    // ✅ TX emit coalesce + hard de-dupe
    // ============================================================
    property bool   _txEmitPending: false
    property string _lastTxModelKey: ""      // hard de-dupe by model content
    property real   _lastTxEmitAtMs: 0       // optional guard
    property string _lastTxDebug: ""

    function scheduleEmitLatestTx() {
        if (viewer._txEmitPending) return
        viewer._txEmitPending = true

        // ✅ Coalesce: no matter how many model signals fire, emit once per event-loop
        Qt.callLater(function() {
            viewer._txEmitPending = false
            viewer.emitLatestTxToKrakenmapval()
        })
    }

    onTxModelChanged: {
        viewer._lastTxModelKey = ""
        viewer._lastTxEmitAtMs = 0
        viewer.scheduleEmitLatestTx()
    }

    // ============================================================
    // MGRS (WGS84) helper
    // ============================================================
    function _mgrsPad(n, width) {
        var s = String(Math.floor(Math.abs(n)))
        while (s.length < width) s = "0" + s
        return s
    }

    function _mgrsLatitudeBandLetter(latDeg) {
        if (!isFinite(latDeg)) return "Z"
        if (latDeg <= -80) return "C"
        if (latDeg >=  84) return "X"
        var bands = "CDEFGHJKLMNPQRSTUVWX"
        var idx = Math.floor((latDeg + 80) / 8)
        if (idx < 0) idx = 0
        if (idx > 19) idx = 19
        return bands.charAt(idx)
    }

    function _mgrsFixZone(latDeg, lonDeg, zone) {
        if (latDeg >= 56 && latDeg < 64 && lonDeg >= 3 && lonDeg < 12) return 32
        if (latDeg >= 72 && latDeg < 84) {
            if      (lonDeg >= 0  && lonDeg < 9 )  return 31
            else if (lonDeg >= 9  && lonDeg < 21)  return 33
            else if (lonDeg >= 21 && lonDeg < 33)  return 35
            else if (lonDeg >= 33 && lonDeg < 42)  return 37
        }
        return zone
    }

    function _latLonToUtm(latDeg, lonDeg) {
        var a = 6378137.0
        var f = 1.0 / 298.257223563
        var e2 = f * (2 - f)
        var ep2 = e2 / (1 - e2)
        var k0 = 0.9996

        var lat = latDeg * Math.PI / 180.0
        var lon = lonDeg * Math.PI / 180.0

        var zone = Math.floor((lonDeg + 180.0) / 6.0) + 1
        zone = _mgrsFixZone(latDeg, lonDeg, zone)

        var lon0Deg = (zone - 1) * 6 - 180 + 3
        var lon0 = lon0Deg * Math.PI / 180.0

        var sinLat = Math.sin(lat)
        var cosLat = Math.cos(lat)
        var tanLat = Math.tan(lat)

        var N = a / Math.sqrt(1 - e2 * sinLat * sinLat)
        var T = tanLat * tanLat
        var C = ep2 * cosLat * cosLat
        var A = cosLat * (lon - lon0)

        var e4 = e2 * e2
        var e6 = e4 * e2
        var M = a * ((1 - e2/4 - 3*e4/64 - 5*e6/256) * lat
                     - (3*e2/8 + 3*e4/32 + 45*e6/1024) * Math.sin(2*lat)
                     + (15*e4/256 + 45*e6/1024) * Math.sin(4*lat)
                     - (35*e6/3072) * Math.sin(6*lat))

        var easting = k0 * N * (A + (1 - T + C) * Math.pow(A,3)/6
                                + (5 - 18*T + T*T + 72*C - 58*ep2) * Math.pow(A,5)/120) + 500000.0

        var northing = k0 * (M + N * tanLat * (A*A/2
                                + (5 - T + 9*C + 4*C*C) * Math.pow(A,4)/24
                                + (61 - 58*T + T*T + 600*C - 330*ep2) * Math.pow(A,6)/720))

        if (latDeg < 0) northing += 10000000.0

        return { zone: zone, easting: easting, northing: northing, latDeg: latDeg, lonDeg: lonDeg }
    }

    function _mgrs100kSetForZone(zone) {
        var set = zone % 3
        if (set === 0) set = 3
        return set
    }

    function _mgrs100kColumnLetter(zone, easting) {
        var set = _mgrs100kSetForZone(zone)
        var colSets = { 1: "ABCDEFGH", 2: "JKLMNPQR", 3: "STUVWXYZ" }
        var cols = colSets[set]
        var col = Math.floor(easting / 100000.0)
        if (col < 0) col = 0
        if (col > 8) col = 8
        var idx = (col - 1) % 8
        if (idx < 0) idx = 0
        return cols.charAt(idx)
    }

    function _mgrs100kRowLetter(zone, northing) {
        var rowSets = { 1: "ABCDEFGHJKLMNPQRSTUV", 2: "FGHJKLMNPQRSTUVABCDE" }
        var set = (zone % 2 === 0) ? 2 : 1
        var rows = rowSets[set]
        var row = Math.floor(northing / 100000.0)
        if (row < 0) row = 0
        var idx = row % 20
        return rows.charAt(idx)
    }

    // digits: 1..5 (1=10km,2=1km,3=100m,4=10m,5=1m)
    function latLonToMgrs(latDeg, lonDeg, digits) {
        var lat = Number(latDeg)
        var lon = Number(lonDeg)
        if (!isFinite(lat) || !isFinite(lon)) return "-"

        var d = (digits === undefined) ? 5 : Math.floor(Number(digits))
        if (d < 1) d = 1
        if (d > 5) d = 5

        var utm = _latLonToUtm(lat, lon)
        var zone = utm.zone
        var band = _mgrsLatitudeBandLetter(lat)

        var e = utm.easting
        var n = utm.northing

        var colL = _mgrs100kColumnLetter(zone, e)
        var rowL = _mgrs100kRowLetter(zone, n)

        var eIn = Math.floor(e % 100000.0)
        var nIn = Math.floor(n % 100000.0)
        if (eIn < 0) eIn += 100000
        if (nIn < 0) nIn += 100000

        var div = Math.pow(10, 5 - d)
        var eRed = Math.floor(eIn / div)
        var nRed = Math.floor(nIn / div)

        var eStr = _mgrsPad(eRed, d)
        var nStr = _mgrsPad(nRed, d)

        return String(zone) + band + " " + colL + rowL + " " + eStr + " " + nStr
    }

    // ============================================================
    // TX -> C++ emit
    // ============================================================
    function emitLatestTxToKrakenmapval() {
        if (!txModel) return

        // ✅ use injected ref only
        if (!viewer.krakenmapval) {
            console.log("[TX] krakenmapval is null (not injected)")
            return
        }

        // ✅ prevent stacked/hidden instances
        if (!viewer.visible || !viewer.loggingEnabled) return

        if (txModel.count !== undefined && txModel.count <= 0) return

        // ✅ latest = index 0 (ตาม UI ของคุณ)
        var m = txModel.get ? txModel.get(0) : null
        if (!m) return

        var lat = Number(m.lat)
        var lon = Number(m.lon)
        var rms = Number(m.rms || 0)
        var updatedMs = Number(m.updatedMs || 0)

        if (!isFinite(lat) || !isFinite(lon)) return

        // ✅ HARD DEDUPE BY MODEL CONTENT
        // Same TX point -> never re-send even if multiple model signals fired.
        var modelKey = lat.toFixed(6) + "," + lon.toFixed(6)
                     + "|rms=" + Math.round(rms)
                     + "|ms=" + Math.floor(updatedMs)

        if (modelKey === viewer._lastTxModelKey) {
            return
        }
        viewer._lastTxModelKey = modelKey

        // freq snapshot (from rf cache)
        var fHz = 0
        if (logger && typeof logger._getRfFreqHzNow === "function")
            fHz = Number(logger._getRfFreqHzNow())

        // date/time (fallback from updatedMs/now)
        var dStr = String(logger ? logger.lastDate : "")
        var tStr = String(logger ? logger.lastTime : "")

        function _pad2(n) { n = Math.floor(Number(n)); return (n < 10 ? ("0" + n) : ("" + n)) }
        function _fmtDate(ms) {
            var d = new Date(Number(ms))
            var mon = ["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"][d.getMonth()]
            return _pad2(d.getDate()) + " " + mon + " " + d.getFullYear()
        }
        function _fmtTime(ms) {
            var d = new Date(Number(ms))
            return _pad2(d.getHours()) + ":" + _pad2(d.getMinutes()) + ":" + _pad2(d.getSeconds())
        }

        var baseMs = (isFinite(updatedMs) && updatedMs > 0) ? updatedMs : Date.now()
        if (!dStr.length) dStr = _fmtDate(baseMs)
        if (!tStr.length) tStr = _fmtTime(baseMs)

        // mgrs
        var mgrs = "-"
        mgrs = latLonToMgrs(lat, lon, 5)

        // call into C++
        if (typeof viewer.krakenmapval.onTxSnapshotUpdated === "function") {
            viewer.krakenmapval.onTxSnapshotUpdated(
                lat, lon, rms, fHz, dStr, tStr, updatedMs, mgrs
            )
        } else {
            console.log("[TX] krakenmapval.onTxSnapshotUpdated not found")
        }
    }

    // Hook txModel signals (minimal set to avoid duplicate triggers)
    Connections {
        target: txModel
        ignoreUnknownSignals: true

        function onRowsInserted(parent, first, last) { viewer.scheduleEmitLatestTx() }
        function onModelReset() { viewer.scheduleEmitLatestTx() }
        function onCountChanged() { viewer.scheduleEmitLatestTx() }
    }

    // ============================================================
    // Visual effects
    // ============================================================
    layer.enabled: true
    layer.effect: DropShadow {
        color: "#00E5FF33"
        radius: 22
        samples: 48
        verticalOffset: 0
        horizontalOffset: 0
    }

    Rectangle {
        anchors.fill: parent
        radius: viewer.radius
        color: "transparent"
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#14212A" }
            GradientStop { position: 1.0; color: "#070B0E" }
        }
        opacity: 0.85
    }

    // ============================================================
    // Logger (DOA)
    // ============================================================
    QtObject {
        id: logger
        signal historyUpdated()

        property string inst: "DH-" + Date.now() + "-" + Math.floor(Math.random()*100000)

        property var doaHistory: []
        property var lastVfoConfig: null
        property string lastTime: ""
        property string lastDate: ""
        property string lastUptime: ""

        property real lastRfFreqHz: 0
        property real lastRfBwHz: 0

        property int  maxStableMs: 3200
        property real maxStableDeltaDeg: 1.0
        property int  maxMinIntervalMs: 300

        property string candKey: ""
        property real   candDoa: -9999
        property real   candConf: -1
        property real   candSinceMs: 0
        property real   candFreqHz: 0
        property real   candBwHz: 0

        property real   lastDeltaDegUsed: -1
        property string lastLoggedKey: ""
        property real   lastLoggedDoa: -9999
        property real   lastLoggedMs: 0

        function saveRfParams(freqHz, bwHz) {
            var f = Number(freqHz)
            var b = Number(bwHz)

            if (viewer.rfCache) {
                if (typeof viewer.rfCache.save === "function") {
                    viewer.rfCache.save(f, b)
                } else {
                    if (isFinite(f) && f > 0) viewer.rfCache.lastRfFreqHz = f
                    if (isFinite(b) && b > 0) viewer.rfCache.lastRfBwHz   = b
                }
            }

            if (viewer.rfCache) {
                var sf = Number(viewer.rfCache.lastRfFreqHz)
                var sb = Number(viewer.rfCache.lastRfBwHz)
                if (isFinite(sf) && sf > 0) lastRfFreqHz = sf
                if (isFinite(sb) && sb > 0) lastRfBwHz   = sb
            } else {
                if (isFinite(f) && f > 0) lastRfFreqHz = f
                if (isFinite(b) && b > 0) lastRfBwHz   = b
            }
        }

        function _getRfFreqHzNow() {
            if (viewer.rfCache && isFinite(viewer.rfCache.lastRfFreqHz) && viewer.rfCache.lastRfFreqHz > 0)
                return Number(viewer.rfCache.lastRfFreqHz)
            if (isFinite(lastRfFreqHz) && lastRfFreqHz > 0)
                return Number(lastRfFreqHz)
            return 0
        }
        function _getRfBwHzNow() {
            if (viewer.rfCache && isFinite(viewer.rfCache.lastRfBwHz) && viewer.rfCache.lastRfBwHz > 0)
                return Number(viewer.rfCache.lastRfBwHz)
            if (isFinite(lastRfBwHz) && lastRfBwHz > 0)
                return Number(lastRfBwHz)
            return 0
        }

        function _now() { return Date.now() }

        function _degChanged(a, b) {
            if (!isFinite(a) || !isFinite(b)) return true
            return Math.abs(Number(a) - Number(b)) >= maxStableDeltaDeg
        }

        function _fmtDoa(v) {
            var x = Number(v)
            if (!isFinite(x)) return "-"
            return x.toFixed(3)
        }
        function _fmtFreqMHzFromHz(hz) {
            var v = Number(hz)
            if (!isFinite(v) || v <= 0) return "-"
            return (v / 1000000.0).toFixed(3)
        }
        function _fmtBwKHzFromHz(hz) {
            var v = Number(hz)
            if (!isFinite(v) || v <= 0) return "-"
            return (v / 1000.0).toFixed(0)
        }

        function pad2(v) { v = Math.floor(v); return (v < 10 ? "0" + v : "" + v) }
        function fmtSysDateTime(ms) {
            var d = new Date(Number(ms))
            var y  = d.getFullYear()
            var mo = pad2(d.getMonth() + 1)
            var da = pad2(d.getDate())
            var hh = pad2(d.getHours())
            var mm = pad2(d.getMinutes())
            var ss = pad2(d.getSeconds())
            return y + "-" + mo + "-" + da + " " + hh + ":" + mm + ":" + ss
        }

        function _appendLog(timestamp, name, frequency, doaValue, extra, rawVfoIndex) {
            doaHistory.push({
                timestamp: timestamp,
                name: name,
               frequency: frequency,
                doa: doaValue + (extra.length ? (" " + extra) : ""),
                rawVfoIndex: rawVfoIndex
            })
            if (doaHistory.length > viewer.maxLogItems)
                doaHistory.shift()

            doaLogModel.insert(0, {
                timestamp: timestamp,
                name: name,
                frequency: frequency,
                doaValue: doaValue,
                extra: extra
            })
            if (doaLogModel.count > viewer.maxLogItems)
                doaLogModel.remove(doaLogModel.count - 1)

            historyUpdated()
        }

        // NOTE: You can keep your Max-DOA logging here as before
        function feedMaxDoaCandidate(obj) {
            if (!obj) return
            if (!viewer.visible || !viewer.loggingEnabled) return

            var nowMs = _now()

            var key  = String(obj.key || "")
            var doa  = Number(obj.doa)
            var conf = Number(obj.confidence)
            if (!isFinite(doa))  doa  = 0
            if (!isFinite(conf)) conf = 0

            // ✅ rate-limit กันถี่เกิน
            if ((nowMs - lastLoggedMs) < maxMinIntervalMs) return

            // ✅ de-dupe: key เดิม + doa ไม่เปลี่ยนเกิน delta -> ไม่ต้อง log
            var sameKey = (key === lastLoggedKey)
            var sameDoa = !(_degChanged(doa, lastLoggedDoa))   // ใช้ maxStableDeltaDeg เป็น threshold
            if (sameKey && sameDoa) return

            // snapshot RF ตอนนี้
            var fNow = _getRfFreqHzNow()
            var bNow = _getRfBwHzNow()

            lastLoggedKey = key
            lastLoggedDoa = doa
            lastLoggedMs  = nowMs

            // เวลา (remote ถ้ามี)
            var hasRemoteTime = (String(lastDate || "").length > 0) && (String(lastTime || "").length > 0)
            var tsRemote = hasRemoteTime ? ("[" + lastDate + " " + lastTime + "]")
                                         : ("[" + fmtSysDateTime(nowMs) + "]")

            var doaStr = _fmtDoa(doa)

            var extra = ""
            if (key.length) extra += "key=" + key
            extra += (extra.length ? " " : "") + "conf=" + Number(conf).toFixed(2)

            if (obj.heading !== undefined && isFinite(Number(obj.heading)))
                extra += " hdg=" + Number(obj.heading).toFixed(1)

            if (obj.lat !== undefined && isFinite(Number(obj.lat)) &&
                obj.lon !== undefined && isFinite(Number(obj.lon))) {
                extra += " lat=" + Number(obj.lat).toFixed(6) + " lon=" + Number(obj.lon).toFixed(6)
            }

            var freqStr = _fmtFreqMHzFromHz(fNow)
            var bwStr   = _fmtBwKHzFromHz(bNow)
            var freqDisplay = (freqStr === "-") ? "-" : (freqStr + " MHz" + (bwStr !== "-" ? (" / " + bwStr + " kHz") : ""))

            _appendLog(tsRemote, "[MAX DOA]", freqDisplay, doaStr, extra, -999)
        }

        function saveDoa(vfoIndex, doaValue) {
            if (!viewer.visible || !viewer.loggingEnabled) return

            let timeStr = "[" + lastDate + " " + lastTime + "]"
            let nameStr = ""
            let doaStr = "-"

            if (vfoIndex === null) {
                nameStr = "[Center Frequency]"
            } else if (vfoIndex >= 0) {
                nameStr = "[VFO-" + vfoIndex + "]"
            }
            if (!isNaN(doaValue)) doaStr = Number(doaValue).toFixed(3)

            _appendLog(timeStr, nameStr, "-", doaStr, "", vfoIndex)
        }

        function saveTime(currentTime, currentDate, uptime) {
            lastTime = currentTime
            lastDate = currentDate
            lastUptime = uptime
        }

        function saveVfoConfig(config) {
            lastVfoConfig = config
        }
    }

    // ============================================================
    // Fade behavior (UI only)
    // ============================================================
    Behavior on opacity {
        NumberAnimation { duration: 220; easing.type: Easing.InOutQuad }
    }

    Timer {
        id: fadeTimer
        interval: 2200
        repeat: false
        onTriggered: { if (!daqLocked) viewer.active = false }
    }

    function triggerFadeIn() {
        active = true
        if (!daqLocked) fadeTimer.restart()
    }

    Component.onCompleted: {
        active = false
        viewer.txCoordMode = viewSettings.txCoordMode
        viewer._lastTxModelKey = ""
        viewer.scheduleEmitLatestTx()
    }

    Connections {
        target: logger
        function onHistoryUpdated() { viewer.triggerFadeIn() }
    }

    function pad2(v) { v = Math.floor(v); return (v < 10 ? "0" + v : "" + v) }
    function tsDateTime(ms) {
        if (!ms) return "-"
        var d = new Date(Number(ms))
        var y = d.getFullYear()
        var mo = pad2(d.getMonth() + 1)
        var da = pad2(d.getDate())
        var hh = pad2(d.getHours())
        var mm = pad2(d.getMinutes())
        var ss = pad2(d.getSeconds())
        return y + "-" + mo + "-" + da + " " + hh + ":" + mm + ":" + ss
    }

    // ===================== TOP BAR =====================
    Row {
        id: topBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        spacing: 10

        Column {
            spacing: 2
            Text {
                text: "DoA / TX Monitor"
                color: "white"
                font.pixelSize: 14
                font.bold: true
            }
            Text {
                text: (txModel && txModel.count !== undefined)
                      ? ("TX points: " + txModel.count)
                      : "TX points: -"
                color: "#A9C1CC"
                font.pixelSize: 10
            }
        }

        Item { width: 1; height: 1; Layout.fillWidth: true }

        Button {
            id: lockFadeButton
            width: 40
            height: 40
            checkable: true
            checked: false

            onClicked: {
                viewer.daqLocked = lockFadeButton.checked
                if (viewer.daqLocked) { viewer.active = true; fadeTimer.stop() }
                else fadeTimer.restart()
            }

            background: Rectangle {
                radius: 12
                color: lockFadeButton.checked ? "#1F6F4A" : "#0E1B22"
                border.width: 1
                border.color: lockFadeButton.checked ? "#2ECC71" : "#314f61"
            }

            contentItem: Image {
                anchors.centerIn: parent
                source: lockFadeButton.checked
                        ? "qrc:/iScreenDFqml/images/lock.png"
                        : "qrc:/iScreenDFqml/images/unlock.png"
                width: 30
                height: 30
                fillMode: Image.PreserveAspectFit
            }
        }
    }

    // ===================== TABS =====================
    Row {
        id: tabs
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.topMargin: 10
        spacing: 10

        property int tab: 0 // 0=TX, 1=DOA LOG

        // ✅ when user returns to TX tab -> emit once
        onTabChanged: {
            if (tab === 0) viewer.scheduleEmitLatestTx()
        }

        Rectangle {
            id: tabBg
            width: 200
            height: 44
            radius: 18
            color: "#0E1B22"
            border.width: 1
            border.color: "#22313A"

            Row {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 6

                Rectangle {
                    id: tabTx
                    width: 90
                    height: parent.height
                    radius: 16
                    color: tabs.tab === 0 ? "#FFB300" : "transparent"
                    border.width: tabs.tab === 0 ? 0 : 1
                    border.color: "#22313A"

                    property bool pressed: false
                    scale: pressed ? 0.98 : 1.0
                    Behavior on scale { NumberAnimation { duration: 70 } }

                    Text {
                        anchors.centerIn: parent
                        text: "TX"
                        color: tabs.tab === 0 ? "#0B1216" : "#A9C1CC"
                        font.bold: true
                        font.pixelSize: 13
                    }

                    TapHandler {
                        onPressedChanged: tabTx.pressed = pressed
                        onTapped: tabs.tab = 0
                    }

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -6
                        onPressed: tabTx.pressed = true
                        onReleased: tabTx.pressed = false
                        onCanceled: tabTx.pressed = false
                        onClicked: tabs.tab = 0
                    }
                }

                Rectangle {
                    id: tabDoa
                    width: 90
                    height: parent.height
                    radius: 16
                    color: tabs.tab === 1 ? "#00FFAA" : "transparent"
                    border.width: tabs.tab === 1 ? 0 : 1
                    border.color: "#22313A"

                    property bool pressed: false
                    scale: pressed ? 0.98 : 1.0
                    Behavior on scale { NumberAnimation { duration: 70 } }

                    Text {
                        anchors.centerIn: parent
                        text: "DOA"
                        color: tabs.tab === 1 ? "#0B1216" : "#A9C1CC"
                        font.bold: true
                        font.pixelSize: 13
                    }

                    TapHandler {
                        onPressedChanged: tabDoa.pressed = pressed
                        onTapped: tabs.tab = 1
                    }

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -6
                        onPressed: tabDoa.pressed = true
                        onReleased: tabDoa.pressed = false
                        onCanceled: tabDoa.pressed = false
                        onClicked: tabs.tab = 1
                    }
                }
            }
        }

        // coord toggle (only show on TX tab)
        Rectangle {
            id: coordToggle
            width: 236
            height: 44
            radius: 18
            color: "#0E1B22"
            border.width: 1
            border.color: "#22313A"
            visible: (tabs.tab === 0)

            Row {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 6

                Rectangle {
                    id: btnLatLon
                    width: 110
                    height: parent.height
                    radius: 16
                    color: viewer.txCoordMode === 0 ? "#FFB300" : "transparent"
                    border.width: viewer.txCoordMode === 0 ? 0 : 1
                    border.color: "#22313A"

                    property bool pressed: false
                    scale: pressed ? 0.98 : 1.0
                    Behavior on scale { NumberAnimation { duration: 70 } }

                    Text {
                        anchors.centerIn: parent
                        text: "lat/lon"
                        color: viewer.txCoordMode === 0 ? "#0B1216" : "#A9C1CC"
                        font.pixelSize: 13
                        font.bold: true
                    }

                    TapHandler {
                        onPressedChanged: btnLatLon.pressed = pressed
                        onTapped: {
                            viewer.txCoordMode = 0
                            viewSettings.txCoordMode = 0
                            viewer._lastTxModelKey = ""   // allow re-emit after mode toggle
                            viewer.scheduleEmitLatestTx()
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -6
                        onPressed: btnLatLon.pressed = true
                        onReleased: btnLatLon.pressed = false
                        onCanceled: btnLatLon.pressed = false
                        onClicked: {
                            viewer.txCoordMode = 0
                            viewSettings.txCoordMode = 0
                            viewer._lastTxModelKey = ""
                            viewer.scheduleEmitLatestTx()
                        }
                    }
                }

                Rectangle {
                    id: btnMgrs
                    width: 110
                    height: parent.height
                    radius: 16
                    color: viewer.txCoordMode === 1 ? "#00FFAA" : "transparent"
                    border.width: viewer.txCoordMode === 1 ? 0 : 1
                    border.color: "#22313A"

                    property bool pressed: false
                    scale: pressed ? 0.98 : 1.0
                    Behavior on scale { NumberAnimation { duration: 70 } }

                    Text {
                        anchors.centerIn: parent
                        text: "MGRS"
                        color: viewer.txCoordMode === 1 ? "#0B1216" : "#A9C1CC"
                        font.pixelSize: 13
                        font.bold: true
                    }

                    TapHandler {
                        onPressedChanged: btnMgrs.pressed = pressed
                        onTapped: {
                            viewer.txCoordMode = 1
                            viewSettings.txCoordMode = 1
                            viewer._lastTxModelKey = ""
                            viewer.scheduleEmitLatestTx()
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -6
                        onPressed: btnMgrs.pressed = true
                        onReleased: btnMgrs.pressed = false
                        onCanceled: btnMgrs.pressed = false
                        onClicked: {
                            viewer.txCoordMode = 1
                            viewSettings.txCoordMode = 1
                            viewer._lastTxModelKey = ""
                            viewer.scheduleEmitLatestTx()
                        }
                    }
                }
            }
        }
    }

    // ===================== CONTENT =====================
    Item {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: tabs.bottom
        anchors.bottom: parent.bottom
        anchors.margins: 14

        // ---------- TX PANEL ----------
        Item {
            anchors.fill: parent
            visible: tabs.tab === 0

            Rectangle {
                anchors.fill: parent
                radius: 14
                color: "#0E1B22"
                border.width: 1
                border.color: "#22313A"
                opacity: 0.95
            }

            Item {
                anchors.fill: parent
                visible: !txModel || (txModel.count !== undefined && txModel.count === 0)

                Column {
                    anchors.centerIn: parent
                    spacing: 6

                    Text {
                        text: "No TX history yet"
                        color: "white"
                        font.pixelSize: 13
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        text: "Waiting for 2+ DOA intersection..."
                        color: "#A9C1CC"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            ListView {
                id: txList
                anchors.fill: parent
                anchors.margins: 10
                clip: true
                spacing: 8
                model: txModel
                visible: txModel && (txModel.count === undefined || txModel.count > 0)

                boundsBehavior: Flickable.StopAtBounds
                flickDeceleration: 1800
                maximumFlickVelocity: 2500

                delegate: Rectangle {
                    width: txList.width
                    height: 70
                    radius: 14
                    color: (index === 0) ? "#14212A" : "#0B1216"
                    border.width: 1
                    border.color: (index === 0) ? "#FFB300" : "#22313A"
                    opacity: 0.97

                    Rectangle {
                        x: 10; y: 16
                        width: 38; height: 38
                        radius: 14
                        color: (index === 0) ? "#FFB300" : "#0E1B22"
                        border.width: 1
                        border.color: (index === 0) ? "#FFB300" : "#22313A"

                        Text {
                            anchors.centerIn: parent
                            text: "#" + (index + 1)
                            color: (index === 0) ? "#0B1216" : "#A9C1CC"
                            font.bold: true
                            font.pixelSize: 11
                        }
                    }

                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: 58
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 4

                        Text {
                            text: (viewer.txCoordMode === 1)
                                  ? ("MGRS " + viewer.latLonToMgrs(Number(model.lat), Number(model.lon), 5))
                                  : ("lat " + Number(model.lat).toFixed(6) + "   lon " + Number(model.lon).toFixed(6))
                            color: "white"
                            font.pixelSize: 12
                            font.bold: true
                            font.family: "Monospace"
                            elide: Text.ElideRight
                            width: txList.width - 58 - 26
                        }

                        Row {
                            spacing: 10

                            Text {
                                text: "rms " + Math.round(Number(model.rms || 0)) + " m"
                                color: "#FFB300"
                                font.pixelSize: 10
                                font.bold: true
                            }

                            Text {
                                text: "time " + viewer.tsDateTime(model.updatedMs)
                                color: "#A9C1CC"
                                font.pixelSize: 10
                                font.family: "Monospace"
                            }
                        }
                    }

                    Rectangle {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.rightMargin: 10
                        width: 6
                        height: parent.height - 18
                        radius: 3
                        color: (index === 0) ? "#FFB300" : "#22313A"
                        opacity: 0.9
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    active: true
                    policy: ScrollBar.AsNeeded
                }
            }
        }

        // ---------- DOA LOG PANEL ----------
        Item {
            id: doaPanel
            anchors.fill: parent
            visible: tabs.tab === 1

            Rectangle {
                anchors.fill: parent
                radius: 14
                color: "#0E1B22"
                border.width: 1
                border.color: "#22313A"
                opacity: 0.95
            }

            property bool stickToTop: true
            property bool userInteracting: false

            Timer {
                id: doaIdleTimer
                interval: 380
                repeat: false
                onTriggered: doaPanel.userInteracting = false
            }

            ListView {
                id: doaList
                anchors.fill: parent
                anchors.margins: 10
                clip: true
                model: doaLogModel
                spacing: 8

                boundsBehavior: Flickable.StopAtBounds
                flickDeceleration: 1800
                maximumFlickVelocity: 2500

                onContentYChanged: {
                    doaPanel.userInteracting = true
                    doaIdleTimer.restart()
                    var topGap = contentY
                    doaPanel.stickToTop = (topGap < 6)
                }

                onCountChanged: {
                    if (!doaPanel.userInteracting && doaPanel.stickToTop) {
                        Qt.callLater(function() { doaList.positionViewAtBeginning() })
                    }
                }

                delegate: Rectangle {
                    width: doaList.width
                    radius: 12
                    color: (index === 0) ? "#14212A" : "#0B1216"
                    border.width: 1
                    border.color: (index === 0) ? "#00FFAA" : "#22313A"
                    opacity: 0.98

                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 6

                        Row {
                            spacing: 8

                            Text {
                                text: model.timestamp
                                color: "#A9C1CC"
                                font.pixelSize: 10
                                font.family: "Monospace"
                            }

                            Text {
                                text: model.name + " [" + model.doaValue + "]"
                                color: "white"
                                font.pixelSize: 12
                                font.bold: true
                                font.family: "Monospace"
                            }

                            Item { width: 1; height: 1; Layout.fillWidth: true }

                            Text {
                                text: "[" + model.frequency + "]"
                                color: "#00FFAA"
                                font.pixelSize: 11
                                font.family: "Monospace"
                            }
                        }

                        Text {
                            visible: String(model.extra).length > 0
                            text: String(model.extra)
                            color: "#6F8C98"
                            font.pixelSize: 10
                            font.family: "Monospace"
                            wrapMode: Text.Wrap
                        }
                    }

                    implicitHeight: Math.max(48, childrenRect.height + 10)
                }

                ScrollBar.vertical: ScrollBar {
                    active: true
                    policy: ScrollBar.AsNeeded
                }
            }

            Rectangle {
                visible: !doaPanel.stickToTop && doaLogModel.count > 0
                width: 120
                height: 30
                radius: 12
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 14
                anchors.topMargin: 14
                color: "#14212A"
                border.width: 1
                border.color: "#00FFAA55"

                Text {
                    anchors.centerIn: parent
                    text: "Follow new"
                    color: "#00FFAA"
                    font.pixelSize: 11
                    font.bold: true
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        doaPanel.stickToTop = true
                        doaPanel.userInteracting = false
                        Qt.callLater(function() { doaList.positionViewAtBeginning() })
                    }
                }
            }

            Item {
                anchors.fill: parent
                visible: doaLogModel.count === 0
                Column {
                    anchors.centerIn: parent
                    spacing: 6
                    Text {
                        text: "No DOA logs yet"
                        color: "white"
                        font.pixelSize: 13
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Text {
                        text: "Waiting for stable max DOA..."
                        color: "#A9C1CC"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }

    // tap to show when faded
    MouseArea {
        anchors.fill: parent
        z: 10000
        visible: !viewer.active
        onPressed: viewer.triggerFadeIn()
        propagateComposedEvents: true
    }
}
