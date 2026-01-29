// WaveEditor.qml — Qt 5.12
import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.12
import QtMultimedia 5.12

Item {
    id: waveEditorRoot
    width: 1200
    height: 260

    /* ===== Public API / States ===== */
    property var  files: []                // absolute paths (normalized)
    property var  samples: []              // downsampled waveform for drawing
    property int  sampleCount: 0
    property int  durationMs: 0            // รวมทุกไฟล์เมื่อ concatMode=true (ms)
    property bool isDarkTheme: true
    property int  maxpointSamples16: 100000000
    property var  segList: []              // [{startSample,endSample,label,durationMs}, ...]
    property bool ended: false
    property bool concatMode: false        // true เมื่อมีมากกว่า 1 ไฟล์
    property var  firstAbs: ""             // path แรก (เผื่อใช้กรณี single file)

    // Totals for display
    property int  totalFiles: 0
    property real totalSizeKB: 0.0
    property real totalDurationSec: 0.0    // seconds (รวมทุกไฟล์)
    property string totalSizeText: "0 KB"
    property real volumeFromMain: convertCurrentVolumeLevel
    property real volumeLevel: 0
    property var  loadedFiles: []
    signal filesReady(var filesArray)
    signal playToggleRequested(bool wantPlay, var filesArray, bool concatMode, int playPosMs)
    // ===== concat playback state =====
    property int  currentSegIndex: 0     // index ใน segList
    property int  segStartMs: 0          // เวลาเริ่มต้นของ segment ปัจจุบันใน timeline รวม
    property bool wantPlay: false

    onVolumeFromMainChanged: {
        volumeLevel = volumeFromMain
        volumeController.value = volumeFromMain
        player.volume = volumeFromMain
        canvas.amplitudeGain = 0.3 + volumeFromMain * 1.7
        canvas.requestPaint()
    }

    // Theme colors
    function theme() {
        return isDarkTheme ? {
            cardBg:        "#161a1d",
            cardFg:        "#e5e7eb",
            subFg:         "#94a3b8",
            rail:          "#2a2f37",
            railBorder:    "#353b45",
            waveColor:     "#12d27a",
            midLineColor:  "#3b3b3b",
            selectColor:   "#00b7ff55",
            playheadColor: "#ffffff",
            frameBorder:   "#000000",
            segBorder:     "#f6ad55",
            segLabel:      "#e5e7eb",
            segBg:         "#00ffffff"
        } : {
            cardBg:        "#ffffff",
            cardFg:        "#111827",
            subFg:         "#4b5563",
            rail:          "#e9eef5",
            railBorder:    "#cad3df",
            waveColor:     "#10b981",
            midLineColor:  "#cbd5e1",
            selectColor:   "#0099ff33",
            playheadColor: "#111827",
            frameBorder:   "#e5e7eb",
            segBorder:     "#f59e0b",
            segLabel:      "#111827",
            segBg:         "#00ffffff"
        }
    }
    property color cardBg:        theme().cardBg
    property color cardFg:        theme().cardFg
    property color subFg:         theme().subFg
    property color rail:          theme().rail
    property color railBorder:    theme().railBorder
    property color waveColor:     theme().waveColor
    property color midLineColor:  theme().midLineColor
    property color selectColor:   theme().selectColor
    property color playheadColor: theme().playheadColor
    property color frameBorder:   theme().frameBorder
    property color segBorder:     theme().segBorder
    property color segLabel:      theme().segLabel
    property color segBg:         theme().segBg

    /* ===== View / Transport State ===== */
    property real zoom: 1.0
    property real scrollPx: 0.0
    property int  playPosMs: 0            // หัวอ่านบนไทม์ไลน์รวม (ms)
    property bool isDraggingPane: false
    property bool isSelecting: false
    property real lastMouseX: 0.0
    property int  selStart: -1
    property int  selEnd: -1
    property real speedInterval: 1.2     // ไล่ playPos ให้ลื่นตา

    property int  _pendingSeekMs: -1
    property bool _pendingResume: false
    property bool enginePlaying: false     // ผู้ใช้กด Play
    property bool autoTimerActive: false   // Timer เดิน updatePlayhead ได้

    Behavior on x {
        enabled: player.playbackState !== Audio.PlayingState
        NumberAnimation { duration: 120; easing.type: Easing.InOutQuad }
    }

    /* ===== Utils ===== */
    function fmtDuration(sec) {
        if (!sec || sec <= 0) return "0.000 s (00:00:00.000)"
        var s = Math.floor(sec)
        var ms = Math.round((sec - s) * 1000)
        var hh = Math.floor(s / 3600)
        var mm = Math.floor((s % 3600) / 60)
        var ss = s % 60
        function pad(n,w){ var t="000"+n; return t.substr(t.length-w) }
        return sec.toFixed(3) + " s (" + pad(hh,2)+":"+pad(mm,2)+":"+pad(ss,2)+"."+pad(ms,3)+")"
    }
    function fmtSizeKB(kb) {
        if (!kb || kb <= 0) return "0 KB"
        return (kb >= 1024 ? (kb/1024).toFixed(3) + " MB" : kb.toFixed(3) + " KB")
    }
    function iconSrc(name) {
        var map = {
            play:       isDarkTheme ? "qrc:/images/playLight.png"     : "qrc:/images/playDark.png",
            pause:      isDarkTheme ? "qrc:/images/puaseLight.png"    : "qrc:/images/puaseDark.png",
            skipLeft:   isDarkTheme ? "qrc:/images/skipLeftLight.png" : "qrc:/images/skipLeftDark.png",
            skipRight:  isDarkTheme ? "qrc:/images/skipRighLight.png" : "qrc:/images/skipRighDark.png"
        }
        return map[name] || ""
    }
    function clamp(v,a,b){ return Math.max(a, Math.min(b, v)) }

    // ใช้เวลา active (single = segList[0], concat = durationMs)
    function activeDurationMs() {
        if (concatMode) return durationMs;
        if (segList.length > 0) return segList[0].durationMs;
        return durationMs;
    }
    function activeDurationSec() { return activeDurationMs() / 1000.0 }

    // mapping sample/pixel
    function contentWidthPx() { return Math.max(1, sampleCount * zoom) }
    function sampleToPx(s){ return s * zoom }
    function contentPxToScreenX(cpx){ return cpx - scrollPx }
    function screenXToContentPx(x){ return x + scrollPx }
    function msToSample(ms){
        var dur = activeDurationMs();
        if (dur <= 0 || sampleCount <= 1) return 0;
        return Math.floor(clamp(ms / dur, 0, 1) * (sampleCount - 1));
    }
    function screenXToSample(x){
        var idx = Math.floor(screenXToContentPx(x) / Math.max(0.0001, zoom))
        return clamp(idx, 0, Math.max(0, sampleCount-1))
    }

    // สร้างพาธ absolute ให้ถูกเสมอ
    function normalizeAbsPath(p) {
        if (!p || !p.length) return "";
        var abs = (""+p);
        if (abs.indexOf("file://") === 0) abs = abs.slice(7);
        if (!/^\/var\/ivoicex\/[^/]+\/\d{8}\//.test(abs)) {
            var base = abs.replace(/^.*\//, "");
            var m = /^([^_]+)_(\d{8})_/.exec(base);
            if (m) abs = "/var/ivoicex/" + m[1] + "/" + m[2] + "/" + base;
        }
        return abs;
    }

    /* ===== Playback helpers (concat) ===== */
    function segBaseMs(idx) {
        var b = 0;
        for (var i = 0; i < idx; ++i) b += segList[i].durationMs;
        return b;
    }

    function findSegByGlobalMs(ms) {
        var acc = 0;
        for (var i = 0; i < segList.length; ++i) {
            var next = acc + segList[i].durationMs;
            if (ms < next) return { index: i, offsetMs: ms - acc };
            acc = next;
        }
        var last = Math.max(0, segList.length - 1);
        return { index: last, offsetMs: segList.length ? segList[last].durationMs : 0 };
    }

    function ensurePlayerSourceForMs(ms) {
        if (!segList || segList.length === 0 || !files || files.length === 0) {
            console.warn("[ensure] no segList/files")
            return
        }

        var f = findSegByGlobalMs(ms)

        if (f.index < 0 || f.index >= files.length) {
            console.warn("[ensure] invalid seg index:", f.index, "files.length=", files.length)
            return
        }

        // ✅ อัปเดต state สำหรับ concat
        currentSegIndex = f.index
        segStartMs = segBaseMs(currentSegIndex)

        var want = "file://" + files[currentSegIndex]

        // ใช้ enginePlaying แทน playbackState เพื่อรู้ว่า "ผู้ใช้ตั้งใจเล่นอยู่"
        var resume = enginePlaying

//        console.log("[ensure] ms=", ms,
//                    " -> seg#", currentSegIndex,
//                    " segStartMs=", segStartMs,
//                    " offsetMs=", f.offsetMs,
//                    " want=", want,
//                    " resume(enginePlaying)=", resume)

        // เปลี่ยนไฟล์ถ้าไม่ตรง
        if (player.source !== want) {
            player.stop()
            player.source = want
//            console.log("[ensure] source set =>", want)
        }

        // seek ได้ทันทีถ้า loaded แล้ว
        if (player.status === Audio.Loaded || player.status === Audio.Buffered || player.duration > 0) {
            var off = Math.max(0, Math.min(segList[currentSegIndex].durationMs - 1, f.offsetMs))
//            console.log("[ensure] instant seek to", off, "ms")
            player.seek(off)
            if (resume) player.play()
        } else {
            _pendingSeekMs = f.offsetMs
            _pendingResume = resume
//            console.log("[ensure] defer seek => pendingSeekMs=", _pendingSeekMs,
//                        " pendingResume=", _pendingResume,
//                        " status=", player.status)
        }
    }

//    function ensurePlayerSourceForMs(ms) {
//        var f = findSegByGlobalMs(ms)
//        if (f.index < 0 || f.index >= files.length) {
//            console.warn("[ensure] invalid seg index:", f.index, "files.length=", files.length)
//            return
//        }

//        var want = "file://" + files[f.index]

//        // ใช้ enginePlaying แทน playbackState เพื่อรู้ว่า "ผู้ใช้ตั้งใจเล่นอยู่"
//        var resume = enginePlaying

//        console.log("[ensure] ms=", ms,
//                    " -> seg#", f.index,
//                    " offsetMs=", f.offsetMs,
//                    " want=", want,
//                    " resume(enginePlaying)=", resume)

//        // เปลี่ยนไฟล์ถ้าไม่ตรง
//        if (player.source !== want) {
//            player.stop()
//            player.source = want
//            console.log("[ensure] source set =>", want)
//        }

//        // ถ้าพร้อม (มี metadata แล้ว) ให้ seek ได้เลย ไม่งั้นดีเลย์ไป onStatusChanged
//        if (player.status === Audio.Loaded || player.status === Audio.Buffered || player.duration > 0) {
//            var off = Math.max(0, Math.min(segList[f.index].durationMs - 1, f.offsetMs))
//            console.log("[ensure] instant seek to", off, "ms")
//            player.seek(off)
//            if (resume) player.play()
//        } else {
//            _pendingSeekMs = f.offsetMs
//            _pendingResume = resume
//            console.log("[ensure] defer seek => pendingSeekMs=", _pendingSeekMs,
//                        " pendingResume=", _pendingResume,
//                        " status=", player.status)
//        }
//    }

    function setPlayhead(ms) {                 // เรียกตอนลากสไลเดอร์/กดข้าม
        playPosMs = Math.max(0, Math.min(durationMs, ms));
        if (concatMode) {
            ensurePlayerSourceForMs(playPosMs);
        } else {
            // single file: ถ้ามีไฟล์ ให้ seek โดยตรงจาก player
            if (files.length > 0) {
                var want = "file://" + files[0];
                if (player.source !== want)
                    player.source = want;
                player.seek(playPosMs);
            }
        }
        updatePlayhead();
    }

    /* ===== WAV parsing (16-bit PCM only) ===== */
    function parseWaveReturn(arrayBuf) {
        var dv = new DataView(arrayBuf)
        if (arrayBuf.byteLength < 44) return {samples:[], durationMs:0}
        var ok = (String.fromCharCode(dv.getUint8(0),dv.getUint8(1),dv.getUint8(2),dv.getUint8(3)) === "RIFF") &&
                 (String.fromCharCode(dv.getUint8(8),dv.getUint8(9),dv.getUint8(10),dv.getUint8(11)) === "WAVE")
        if (!ok) return {samples:[], durationMs:0}

        var offset=12, dataOffset=0, dataSize=0, sampleRate=44100, bitsPerSample=16
        while (offset+8 <= dv.byteLength) {
            var id = String.fromCharCode(dv.getUint8(offset),dv.getUint8(offset+1),dv.getUint8(offset+2),dv.getUint8(offset+3))
            var size = dv.getUint32(offset+4,true); offset += 8
            if (id === "fmt ") {
                bitsPerSample      = dv.getUint16(offset + 14, true)
                sampleRate         = dv.getUint32(offset + 4, true)
            } else if (id==="data") {
                dataOffset=offset; dataSize=size; break
            }
            offset += size
        }
        if (bitsPerSample !== 16 || dataSize<=0) return {samples:[], durationMs:0}

        var samples16 = Math.floor(dataSize/2)
        var step = Math.max(1, Math.floor(samples16/maxpointSamples16))
        var out=[]
        for (var i=0;i<samples16;i+=step){
            var pos=dataOffset + i*2
            if (pos+2>dv.byteLength) break
            var v=dv.getInt16(pos,true)/32768.0
            out.push(v)
        }
        var durMs = Math.floor((samples16/sampleRate)*1000)
        return {samples: out, durationMs: durMs}
    }

    // utility single-file generator (ยังคงไว้ใช้)
    function generateWaveform(fullPath, index) {
        var abs = normalizeAbsPath(fullPath);
        var data = (typeof fileReader !== "undefined") ? fileReader.readFile(abs) : null;
        if (!data || data.byteLength < 44) return;

        var one = parseWaveReturn(data);
        samples = one.samples;
        sampleCount = samples.length;
        segList = [{
            startSample: 0, endSample: sampleCount-1,
            label: abs.replace(/^.*\//,""),
            durationMs: one.durationMs
        }];
        totalFiles = 1;
        files = [abs];
        totalSizeKB = 0;
        totalDurationSec = one.durationMs/1000.0;

        // reset & prepare player
        player.stop();
        player.source = "file://" + abs;
        concatMode = false;

        durationMs = one.durationMs;
        playPosMs = 0;
        fitZoom();
        canvas.requestPaint();
    }

    /* ===== File intake (Combine) ===== */
    function setFiles(items, restoreSummary) {
//        console.log("setFiles:", items)

        var arr = items || []
        loadedFiles = arr
//        console.log("[WaveEditor] setFiles:", arr.length, "file(s)")

        // --- 1) sort ตาม timestamp ในชื่อไฟล์ ---
        var tmp = []
        for (var i = 0; i < arr.length; ++i) {
            var it = arr[i]
            var p  = (typeof it === "string")
                     ? it
                     : (it.full_path || it.path || it.filename || "")
            if (!p) continue

            var base = ("" + p).replace(/^.*\//, "")
            var m    = base.match(/_(\d{8})_(\d{6})_/)
            var key  = m ? Number(m[1] + m[2]) : Number.MAX_SAFE_INTEGER

            tmp.push({ item: it, path: p, base: base, sortKey: key })
        }

        tmp.sort(function(a, b) { return a.sortKey - b.sortKey })

        // --- 2) รวม waveform ---
        var sumKB  = 0.0
        var sumSec = 0.0
        var paths  = []
        var allSamples = []
        var segs = []
        var cursorSample = 0

        for (var t = 0; t < tmp.length; ++t) {
            var it  = tmp[t].item
            var abs = normalizeAbsPath(tmp[t].path)
            paths.push(abs)

            // ====== sizeKB (ถ้ามี metadata แนบมา) ======
            var kb = 0.0
            if (it && it.size_bytes !== undefined && it.size_bytes !== "") {
                var bytes = Number(it.size_bytes)
                if (isFinite(bytes) && bytes > 0) kb = bytes / 1024.0
            } else if (it && it.size !== undefined && it.size !== "") {
                var s = String(it.size)
                var mm = s.match(/[\d.]+/)
                if (mm) {
                    var v = Number(mm[0])
                    if (isFinite(v) && v > 0) kb = v
                }
            }
            sumKB += kb

            // ====== durationMs จาก wavMeta เสมอ ======
            var durMs = -1
            if (typeof fileReader !== "undefined" && fileReader.wavMeta) {
                var meta = fileReader.wavMeta(abs)
                if (meta && meta.ok && meta.duration_ms > 0)
                    durMs = meta.duration_ms
            }

            var data = (typeof fileReader !== "undefined")
                       ? fileReader.readFile(abs)
                       : null
            if (!data || data.byteLength < 44)
                continue

            var one = parseWaveReturn(data)

            if (durMs < 0 || !isFinite(durMs) || durMs <= 0)
                durMs = one.durationMs

            var startS = cursorSample
            for (var j = 0; j < one.samples.length; ++j)
                allSamples.push(one.samples[j])
            cursorSample += one.samples.length

            segs.push({
                startSample: startS,
                endSample:   startS + (one.samples.length - 1),
                label:       abs.replace(/^.*\//,""),
                durationMs:  durMs
            })

            sumSec += durMs / 1000.0
        }

        // --- 3) Commit state ---
        files            = paths
        segList          = segs
        samples          = allSamples
        sampleCount      = allSamples.length
        totalFiles       = paths.length

        // ค่า default จากการคำนวณเอง
        totalSizeKB      = sumKB
        totalDurationSec = sumSec

        var totalMs = 0
        for (var s = 0; s < segs.length; ++s)
            totalMs += segs[s].durationMs
        durationMs = totalMs

        // =========================================================
        // ✅ OVERRIDE จาก summary ที่โหลดมา (ถ้ามี)
        // =========================================================
        if (restoreSummary) {
            // รองรับทั้ง string/number
            var rsTotalMs  = Number(restoreSummary.totalMs)
            var rsDurSec   = Number(restoreSummary.totalDurationSec)
            var rsSizeKB   = Number(restoreSummary.totalSizeKB)

            if (isFinite(rsTotalMs) && rsTotalMs > 0) {
                durationMs = rsTotalMs
            }
            if (isFinite(rsDurSec) && rsDurSec > 0) {
                totalDurationSec = rsDurSec
            }
            if (isFinite(rsSizeKB) && rsSizeKB > 0) {
                totalSizeKB = rsSizeKB
            }

//            console.log("[WaveEditor] APPLY RESTORE SUMMARY ->",
//                        "totalMs=", durationMs,
//                        "totalDurationSec=", totalDurationSec,
//                        "totalSizeKB=", totalSizeKB)
        }

//        console.log("[SUMMARY] totalFiles=", totalFiles,
//                    "sampleCount=", sampleCount,
//                    "totalMs=", durationMs,
//                    "totalDurationSec=", totalDurationSec.toFixed(3),
//                    "totalSizeKB=", (isFinite(totalSizeKB) ? totalSizeKB.toFixed(3) : totalSizeKB),
//                    "samples.length:", samples.length)

//        console.log("[PLAYER] total selected:", files.length, "file(s)")
        for (var idx = 0; idx < files.length; ++idx)
//            console.log("   [" + idx + "]", files[idx])

        concatMode = (files.length > 1)

        player.stop()
        player.source = ""
        ended = false
        enginePlaying = false

        firstAbs = (files.length > 0) ? normalizeAbsPath(files[0]) : ""
        playPosMs = 0

        fitZoom()
        canvas.requestPaint()
    }

//    function setFiles(items) {
//        console.log("setFiles:", items)

//        // กัน null / undefined
//        var arr = items || []
//        loadedFiles = arr
//        console.log("[WaveEditor] setFiles:", arr.length, "file(s)")

//        // --- 1) sort ตาม timestamp ในชื่อไฟล์ ---
//        var tmp = []
//        for (var i = 0; i < arr.length; ++i) {
//            var it = arr[i]
//            var p  = (typeof it === "string")
//                     ? it
//                     : (it.full_path || it.path || it.filename || "")
//            if (!p)
//                continue

//            var base = ("" + p).replace(/^.*\//, "")         // ตัด path เอาแค่ชื่อไฟล์
//            var m    = base.match(/_(\d{8})_(\d{6})_/)       // หา YYYYMMDD, HHMMSS
//            var key  = m ? Number(m[1] + m[2])               // 20250922 + 140039 → 20250922140039
//                         : Number.MAX_SAFE_INTEGER           // ถ้า parse ไม่ได้ → ดันไปท้าย

//            tmp.push({
//                item:    it,
//                path:    p,
//                base:    base,
//                sortKey: key
//            })
//        }

//        // เรียงจากเก่า → ใหม่
//        tmp.sort(function(a, b) {
//            return a.sortKey - b.sortKey
//        })

//        // --- 2) รวม waveform ---
//        var sumKB  = 0.0
//        var sumSec = 0.0
//        var paths  = []
//        var allSamples = []
//        var segs = []
//        var cursorSample = 0

//        for (var t = 0; t < tmp.length; ++t) {
//            var it  = tmp[t].item
//            var pIn = tmp[t].path
//            var abs = normalizeAbsPath(pIn)
//            paths.push(abs)

//            // ====== คำนวณ size (KB) ======
//            var kb = 0.0

//            // 1) ถ้ามี size_bytes (จาก C++ JSON)
//            if (it && it.size_bytes !== undefined && it.size_bytes !== "") {
//                var bytes = Number(it.size_bytes)
//                if (isFinite(bytes) && bytes > 0)
//                    kb = bytes / 1024.0
//            }
//            // 2) ถ้าไม่มี ให้ลอง parse จาก it.size ที่เป็น "52.9 KB"
//            else if (it && it.size !== undefined && it.size !== "") {
//                var s = String(it.size)          // เช่น "52.9 KB"
//                var m = s.match(/[\d.]+/)        // ดึงเฉพาะตัวเลข "52.9"
//                if (m) {
//                    var v = Number(m[0])
//                    if (isFinite(v) && v > 0)
//                        kb = v                   // ถือว่าเป็น KB แล้ว
//                }
//            }

//            sumKB += kb
//            // ====== จบส่วน size ======

//            // หา durationMs จาก wav เสมอ เพื่อให้ Save/Restore ได้ค่าเดียวกัน
//            var durMs = -1
//            if (typeof fileReader !== "undefined" && fileReader.wavMeta) {
//                var meta = fileReader.wavMeta(abs)
//                if (meta && meta.ok && meta.duration_ms > 0)
//                    durMs = meta.duration_ms
//            }

//            var data = (typeof fileReader !== "undefined")
//                       ? fileReader.readFile(abs)
//                       : null
//            if (!data || data.byteLength < 44)
//                continue

//            var one = parseWaveReturn(data)

//            // fallback ถ้า wavMeta ใช้ไม่ได้
//            if (durMs < 0 || !isFinite(durMs) || durMs <= 0)
//                durMs = one.durationMs

////            var durMs = -1
////            if (it && it.duration_sec !== undefined && it.duration_sec !== "") {
////                var d = Number(it.duration_sec)
////                if (isFinite(d) && d > 0)
////                    durMs = Math.round(d * 1000)
////            }

////            if (durMs < 0 && typeof fileReader !== "undefined" && fileReader.wavMeta) {
////                var meta = fileReader.wavMeta(abs)
////                if (meta && meta.ok && meta.duration_ms > 0)
////                    durMs = meta.duration_ms
////            }

////            var data = (typeof fileReader !== "undefined")
////                       ? fileReader.readFile(abs)
////                       : null
////            if (!data || data.byteLength < 44)
////                continue

////            var one = parseWaveReturn(data)
////            if (durMs < 0 || !isFinite(durMs) || durMs <= 0)
////                durMs = one.durationMs

//            var startS = cursorSample
//            for (var j = 0; j < one.samples.length; ++j)
//                allSamples.push(one.samples[j])
//            cursorSample += one.samples.length

//            segs.push({
//                startSample: startS,
//                endSample:   startS + (one.samples.length - 1),
//                label:       abs.replace(/^.*\//,""),
//                durationMs:  durMs
//            })

//            sumSec += durMs / 1000.0
//        }

//        // --- 3) Commit state ลงตัวแปรภายใน WaveEditor ---
//        files            = paths
//        segList          = segs
//        samples          = allSamples
//        sampleCount      = allSamples.length
//        totalFiles       = paths.length
//        totalSizeKB      = sumKB
//        totalDurationSec = sumSec

//        // รวมเวลาทั้งหมด (ms)
//        var totalMs = 0
//        for (var s = 0; s < segs.length; ++s)
//            totalMs += segs[s].durationMs
//        durationMs = totalMs

//        console.log("[SUMMARY] totalFiles=", totalFiles,
//                    "sampleCount=", sampleCount,
//                    "totalMs=", totalMs,
//                    "totalDurationSec=", totalDurationSec.toFixed(3),
//                    "samples.length:", samples.length)

//        // แสดงรายการไฟล์ (debug)
//        console.log("[PLAYER] total selected:", files.length, "file(s)")
//        for (var idx = 0; idx < files.length; ++idx)
//            console.log("   [" + idx + "]", files[idx])

//        // โหมด concat = true เมื่อเลือกหลายไฟล์
//        concatMode = (files.length > 1)

//        // --- 4) เตรียม player / UI ---
//        player.stop()
//        player.source = ""
//        ended = false
//        enginePlaying = false

//        firstAbs = (files.length > 0) ? normalizeAbsPath(files[0]) : ""
//        playPosMs = 0

//        fitZoom()
//        canvas.requestPaint()

//}



    /* ===== Player (QtMultimedia.Audio) ===== */
//    Audio {
//        id: player
//        autoLoad: true
//        volume: waveEditorRoot.volumeLevel

//        onPlaybackStateChanged: {
//            console.log("[Audio] playbackState=", playbackState,
//                        "pos=", position,
//                        "concatMode=", concatMode,
//                        "playPosMs=", playPosMs,
//                        "duration=", duration,
//                        "durationMs(total)=", durationMs)
//            if (!concatMode && playbackState === Audio.StoppedState &&
//                    playPosMs < durationMs && duration > 0 &&
//                    Math.abs(duration*2 - durationMs) <= 10) {
//                console.warn("[Audio] stopped early due to half-duration issue")
//            }
//        }

//        onDurationChanged: {
//            console.log("[Audio] durationChanged:", duration,
//                        " ourDurationMs:", durationMs,
//                        " parsed(seg0):", segList.length ? segList[0].durationMs : -1)
//            if (!concatMode && duration > 0 &&
//                    Math.abs(duration*2 - durationMs) <= 10) {
//                console.warn("[Audio] backend_duration = ~half of expected => suspect WAV header mismatch")
//            }
//        }

//        onPositionChanged: {
//            var dur = activeDurationMs();
//            if (!concatMode) {
//                if (dur > 0 && position >= dur - 1) {
//                    ended = true
//                    player.stop()
//                    player.seek(0)
//                    playPosMs = 0
//                    return
//                }
//                ended = false
//                playPosMs = position
//                updatePlayhead()
//            }
//        }
//        onStatusChanged: {
//             console.log("[Audio] status=", status,
//                         "duration=", duration,
//                         "pos=", position,
//                         "concatMode=", concatMode,
//                         "playPosMs=", playPosMs,
//                         "durationMs(total)=", durationMs)

//             // 1) ---- จัดการ pending seek / resume (ใช้ของเดิม) ----
//             if (_pendingSeekMs >= 0 &&
//                 (status === Audio.Loaded || status === Audio.Buffered || duration > 0)) {

//                 var off = Math.max(0, Math.min(duration - 1, _pendingSeekMs))
//                 console.log("[Audio] do pending seek:", off, "ms; resume=", _pendingResume)

//                 seek(off)
//                 if (_pendingResume)
//                     play()

//                 _pendingSeekMs = -1
//                 _pendingResume = false
//             }

//             // 2) ---- ถ้าไม่ได้ต่อไฟล์ (concatMode = false) ก็ไม่ต้องทำ playlist logic ----
//             if (!concatMode)
//                 return

//             // 3) ---- ไฟล์นี้จบแล้ว → ลองข้ามไป segment ถัดไป ----
//             if (status === Audio.EndOfMedia) {
//                 console.log("[Audio] EndOfMedia reached, enginePlaying=", enginePlaying)
//                 if (enginePlaying && (playPosMs + 10 < durationMs)) {
//                     var nextMs = playPosMs
//                     console.log("[Audio] auto-advance to next segment, nextMs =", nextMs)
//                     ensurePlayerSourceForMs(nextMs)
//                 } else {
//                     console.log("[Audio] all segments finished")
//                     ended = true
//                     enginePlaying = false
//                 }
//             }
//         }
////        onStatusChanged: {
////            console.log("[Audio] status=", status, "duration=", duration, "pos=", position)
////            if ((status === Audio.Loaded || status === Audio.Buffered) && _pendingSeekMs >= 0) {
////                var f = findSegByGlobalMs(playPosMs)
////                var off = Math.max(0, Math.min(segList[f.index].durationMs - 1, _pendingSeekMs))
////                console.log("[Audio] do pending seek:", off, "ms; resume=", _pendingResume)
////                player.seek(off)
////                if (_pendingResume && enginePlaying) player.play()
////                _pendingSeekMs = -1
////                _pendingResume = false
////            }
////        }
//    }
    Audio {
        id: player
        autoLoad: true
        volume: waveEditorRoot.volumeLevel

        // ✅ สำคัญ: อัปเดต playPosMs ใน concat ด้วย (จาก player.position จริง)
        onPositionChanged: {
            if (!enginePlaying) return
            if (durationMs <= 0) return

            if (concatMode) {
                // playPosMs = เวลาเริ่ม seg + ตำแหน่งในไฟล์ปัจจุบัน
                playPosMs = segStartMs + position
                if (playPosMs < 0) playPosMs = 0
                if (playPosMs > durationMs) playPosMs = durationMs
                ended = false
                updatePlayhead()
            } else {
                // single file (ของเดิมคุณ)
                var dur = activeDurationMs()
                if (dur > 0 && position >= dur - 1) {
                    ended = true
                    player.stop()
                    player.seek(0)
                    playPosMs = 0
                    updatePlayhead()
                    return
                }
                ended = false
                playPosMs = position
                updatePlayhead()
            }
        }

        onPlaybackStateChanged: {
//            console.log("[Audio] playbackState=", playbackState,
//                        "pos=", position,
//                        "concatMode=", concatMode,
//                        "playPosMs=", playPosMs,
//                        "duration=", duration,
//                        "durationMs(total)=", durationMs)
        }

        onDurationChanged: {
//            console.log("[Audio] durationChanged:", duration,
//                        " ourDurationMs:", durationMs,
//                        " parsed(seg0):", segList.length ? segList[0].durationMs : -1)
        }

        onStatusChanged: {
//            console.log("[Audio] status=", status,
//                        "duration=", duration,
//                        "pos=", position,
//                        "concatMode=", concatMode,
//                        "playPosMs=", playPosMs,
//                        "durationMs(total)=", durationMs,
//                        "seg#", currentSegIndex)

            // ---- pending seek (ของเดิมคุณ แต่ปรับ clamp ให้ใช้ seg duration ใน concat) ----
            if (_pendingSeekMs >= 0 &&
                (status === Audio.Loaded || status === Audio.Buffered || duration > 0)) {

                var maxDur = duration
                if (concatMode && segList && segList.length > currentSegIndex) {
                    var segDur = Number(segList[currentSegIndex].durationMs || 0)
                    if (isFinite(segDur) && segDur > 0) maxDur = segDur
                }

                var off = Math.max(0, Math.min(maxDur - 1, _pendingSeekMs))
//                console.log("[Audio] do pending seek:", off, "ms; resume=", _pendingResume)

                seek(off)
                if (_pendingResume)
                    play()

                _pendingSeekMs = -1
                _pendingResume = false
            }

            if (!concatMode)
                return

            // ✅ EndOfMedia: ไป segment ถัดไปจริง
            if (status === Audio.EndOfMedia) {
//                console.log("[Audio] EndOfMedia reached, enginePlaying=", enginePlaying, "currentSegIndex=", currentSegIndex)

                if (!enginePlaying) {
                    // ผู้ใช้ไม่ได้กด play ค้างไว้
                    return
                }

                var nextIndex = currentSegIndex + 1
                if (nextIndex < files.length && nextIndex < segList.length) {
                    var nextMs = segBaseMs(nextIndex) // ✅ เวลาเริ่มของ segment ถัดไปใน timeline รวม
//                    console.log("[Audio] advance -> nextIndex=", nextIndex, "nextMs=", nextMs)

                    // setPlayhead จะเรียก ensurePlayerSourceForMs และ seek ให้เอง
                    setPlayhead(nextMs)

                    // กันบางเคสที่ status เปลี่ยนแล้วไม่ auto play
                    player.play()
                } else {
//                    console.log("[Audio] all segments finished")
                    ended = true
                    enginePlaying = false
                    player.stop()
                    player.seek(0)
                    playPosMs = 0
                    updatePlayhead()
                }
            }
        }
    }

    Timer {
        id: tickTimer
        interval: 50
        repeat: true
        running: false
//        running: enginePlaying && concatMode

        onTriggered: {
            if (files.length === 0 || durationMs <= 0) return

            // เดิน playPos เอง ไม่พึ่ง player.duration
            playPosMs = Math.min(durationMs, playPosMs + interval)

            // ถ้า Audio หยุดเร็ว แต่เรายังไม่ถึงปลายจริง ให้สั่งเล่นต่อ
            if (!concatMode && player.playbackState === Audio.StoppedState && !ended && playPosMs < durationMs) {
//                console.log("[tick] single-file: backend stopped early, resume play() to keep clock running")
                player.play()
            }

            if (playPosMs >= durationMs) {
                playPosMs = durationMs
                ended = true
                if (player.playbackState !== Audio.StoppedState) player.stop()
            }

            updatePlayhead()
        }
    }

    // ไล่ playPos ให้ลื่นตา (ทั้ง single/concat)
    Timer {
        id: playheadTimer
        interval: 16
        repeat: true
        running: player.playbackState === Audio.PlayingState && !concatMode
//        running: player.playbackState === Audio.PlayingState && !concatMode
        onTriggered: {
            var dur = activeDurationMs()
            if (dur > 0) {
                var target = durationMs
                var diff = target - playPosMs
                var maxStep = interval * speedInterval
                var step = Math.max(-maxStep, Math.min(maxStep, diff))
                playPosMs = clamp(playPosMs + step, 0, dur)
//                console.log("[playheadTimer]->","target:",target,"diff:",diff,"maxStep:",maxStep,"step:",step,"playPosMs:",playPosMs,"player.position:",player.position,"durationMs:",durationMs,"dur:",dur)
            }
            updatePlayhead()
        }
    }

    /* ===== UI helpers ===== */
    function updatePlayhead(){
        ensurePlayheadVisible()
        var dur = activeDurationMs()
        progress.value = dur > 0 ? (playPosMs / dur) : 0
        canvas.requestPaint()
    }
    function ensurePlayheadVisible(){
        var s = msToSample(playPosMs)
        var cpx = sampleToPx(s)
        var left = scrollPx, right = scrollPx + canvas.width
        if (cpx < left + 100)  scrollPx = clamp(cpx - 100, 0, Math.max(0, contentWidthPx()-canvas.width))
        else if (cpx > right - 100) scrollPx = clamp(cpx - canvas.width + 100, 0, Math.max(0, contentWidthPx()-canvas.width))
    }
    function seekToSample(s){
        if (durationMs>0 && sampleCount>1) {
            var ms = (s/(sampleCount-1))*durationMs
            if (concatMode) setPlayhead(ms)
            else {
                player.seek(ms)
                playPosMs = ms
            }
        }
        ensurePlayheadVisible()
        canvas.requestPaint()
    }
    function nudgeMs(delta){
        if (durationMs<=0) return
        var newMs = clamp(playPosMs + delta, 0, durationMs)
        if (concatMode) setPlayhead(newMs)
        else {
            player.seek(newMs)
            playPosMs = newMs
        }
        ensurePlayheadVisible()
        canvas.requestPaint()
    }
    function fitZoom(){
        if (sampleCount<=0) return
        zoom = Math.max(0.001, waveCard.width / sampleCount)
        scrollPx = 0
        canvas.requestPaint()
    }
    function fitToSelection(){
        if (selStart<0 || selEnd<=selStart) return
        var span = selEnd - selStart + 1
        zoom = Math.max(0.001, waveCard.width / span)
        scrollPx = clamp(sampleToPx(selStart), 0, Math.max(0, contentWidthPx()-waveCard.width))
        canvas.requestPaint()
    }
    function zoomAt(screenX, factor){
        var oldZoom = zoom
        var newZoom = clamp(oldZoom*factor, 0.001, 50)
        if (newZoom === oldZoom) return
        var cpxBefore = screenXToContentPx(screenX)
        zoom = newZoom
        var cpxAfter = screenXToContentPx(screenX)
        scrollPx = clamp(scrollPx + (cpxBefore - cpxAfter), 0, Math.max(0, contentWidthPx()-waveCard.width))
        canvas.requestPaint()
    }

    /* ===== Reset waveform when Clear pressed ===== */
    function clearWaveform() {
//        console.log("[WaveEditor] clearWaveform() called")
        player.stop()
        player.source = ""
        samples = []
        sampleCount = 0
        segList = []
        files = []
        totalFiles = 0
        totalSizeKB = 0
        totalDurationSec = 0
        durationMs = 0
        playPosMs = 0
        ended = false
        concatMode = false
        enginePlaying = false
        canvas.requestPaint()
    }

    /* ===== Card / UI ===== */
    Rectangle {
        id: card
        anchors.fill: parent
        anchors.margins: 8
        radius: 14
        color: cardBg
        border.color: frameBorder
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            // Row 1 : Title • Player • Volume
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Text {
                    text: "Wave Editor"
                    color: cardFg
                    font.pixelSize: 16
                    Layout.alignment: Qt.AlignVCenter
                }

                Item { Layout.fillWidth: true }

                // Total / Now (ยึด timeline รวม)
                Label {
                    id: totalsLabel
                    color: subFg
                    font.pixelSize: 20
                    Layout.alignment: Qt.AlignVCenter
                    text: {
                        const totalSec = (durationMs > 0 ? durationMs/1000.0 : 0);
                        const nowSec   = Math.max(0, Math.min(totalSec, playPosMs/1000.0));
                        const pct      = (totalSec > 0) ? Math.round((nowSec/totalSec)*1000)/10 : 0;
                        "Total: " + totalFiles + " files • " +
                        fmtSizeKB(totalSizeKB) + " • " +
                        fmtDuration(totalSec) + "  |  " +
                        "Now: " + fmtDuration(nowSec) + " (" + pct + "%)"
                    }
                }

                // Transport

                PlayerController {
                    id: transport
                    isDarkTheme: waveEditorRoot.isDarkTheme
                    playing: player.playbackState === Audio.PlayingState
                    stepMs: 1000
                    Layout.alignment: Qt.AlignVCenter

                    onPrevRequested: nudgeMs(-stepMs)
                    onNextRequested: nudgeMs(+stepMs)
                    onTogglePlayRequested: function (wantPlay) {
                        enginePlaying = wantPlay

//                        console.log("[PlayerController] onTogglePlayRequested wantPlay=", wantPlay,
//                                    "ended=", ended, "concatMode=", concatMode,
//                                    "files.length=", files.length,
//                                    "durationMs(before)=", durationMs,
//                                    "player.source=", player.source)

                        if (wantPlay) {
                            if (ended) { setPlayhead(0); ended = false; }

                            if (concatMode) {
                                ensurePlayerSourceForMs(playPosMs)
                            } else {
                                if (files.length > 0) {
                                    var want = "file://" + files[0];
                                    if (player.source !== want) {
                                        player.stop();
                                        player.source = want;
                                    }
                                }
                            }
                            player.play()
                        } else {
                            player.pause()
                            wsClient.setSpeakerVolumeMute(1)
//                            console.log("set scanSqlLevels::", scanSqlLevels)
                            mainWindows.setSqlLevel(scanSqlLevels)
//                            console.log("set scanSqlLevels::", scanSqlLevels)
                            mainWindows.sendmessage('{"type": "dspcontrol","params": {"squelch_level": '+((scanSqlLevels-255)/2).toFixed(1)+'}}')
                            scanSqlLevels = 0
                        }
                    }
                }

                RowLayout {
                    spacing: 8
                    Layout.alignment: Qt.AlignVCenter

                    Text {
                        text: "Vol"
                        color: subFg
                        Layout.alignment: Qt.AlignVCenter
                    }
                    Slider {
                        id: volumeController
                        Layout.preferredWidth: 160
                        from: 0
                        to: 1
                        // volumeLevel เป็น property ของ root (waveEditorRoot)
                        value: waveEditorRoot.volumeLevel

                        onValueChanged: {
                            waveEditorRoot.volumeLevel = value
                            player.volume = value
                            canvas.amplitudeGain = 0.3 + value * 1.7
                            canvas.requestPaint()
                        }
                    }

                    // === ตัวเลขด้านขวา ===
                    Text {
                        id: volText
                        text: Math.round(volumeController.value * 100) + "%"
                        color: subFg
                        font.pixelSize: 14
                        Layout.alignment: Qt.AlignVCenter
                    }
                }
            }

            // Row 2 : Progress (scrub)
            Slider {
                id: progress
                Layout.fillWidth: true
                from: 0; to: 1; value: 0
                background: Rectangle { anchors.fill: parent; color: rail; radius:6; border.color: railBorder }
                handle: Rectangle { width: 12; height: 12; radius: 6; color: cardFg }
                onPressedChanged: {
                    if (pressed && durationMs>0) player.pause()
                    if (!pressed && durationMs>0 && enginePlaying) player.play()
                }
                onMoved: {
                    var dur = activeDurationMs();
                    if (dur > 0) setPlayhead(value * dur);
                }
            }

            // Row 3 : Waveform
            Rectangle {
                id: waveCard
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 10
                color: isDarkTheme ? "#0f1318" : "#f8fafc"
                border.color: isDarkTheme ? "#0b0e12" : "#e5e7eb"

                Canvas {
                    id: canvas
                    anchors.fill: parent
                    antialiasing: true
                    property real amplitudeGain: 0.3

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0,0,width,height)

                        // baseline
                        ctx.strokeStyle = midLineColor
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        ctx.moveTo(0, height/2)
                        ctx.lineTo(width, height/2)
                        ctx.stroke()

                        if (sampleCount<=0) return

                        // ===== กรอบแบ่งไฟล์ (segment) =====
                        for (var si=0; si<segList.length; ++si) {
                            var seg = segList[si]
                            var x0 = contentPxToScreenX(sampleToPx(seg.startSample))
                            var x1 = contentPxToScreenX(sampleToPx(seg.endSample+1))
                            var L = Math.max(0, Math.min(x0, x1))
                            var R = Math.min(width, Math.max(x0, x1))
                            if (R <= L) continue

                            ctx.fillStyle = segBg
                            ctx.fillRect(L, 0, R-L, height)

                            ctx.strokeStyle = segBorder
                            ctx.lineWidth = 1
                            ctx.strokeRect(L+0.5, 0.5, (R-L)-1, height-1)

                            var base = seg.label || ""
                            var nameY = height - 4
                            ctx.fillStyle = segLabel
                            ctx.font = "10px sans-serif"
                            var show = base
                            while (ctx.measureText(show).width > (R-L-8) && show.length > 5)
                                show = show.slice(0, show.length-4) + "…"
                            ctx.fillText(show, L+4, nameY)
                        }

                        // ===== selection =====
                        if (selStart>=0 && selEnd>selStart) {
                            var sx0 = contentPxToScreenX(sampleToPx(selStart))
                            var sx1 = contentPxToScreenX(sampleToPx(selEnd))
                            var SL = Math.max(0, Math.min(sx0,sx1))
                            var SR = Math.min(width, Math.max(sx0,sx1))
                            ctx.fillStyle = selectColor
                            if (SR>SL) ctx.fillRect(SL, 0, SR-SL, height)
                        }

                        // ===== waveform (column-based) =====
                        ctx.fillStyle = waveColor
                        var mid = height/2
                        var pxPerSample = Math.max(0.0001, zoom)

                        for (var x = 0; x < width; ++x) {
                            var sampleIdx = Math.floor((scrollPx + x)/pxPerSample)
                            if (sampleIdx < 0 || sampleIdx >= sampleCount)
                                continue
                            var v = Math.abs(samples[sampleIdx] || 0)
                            // ใช้ amplitudeGain คูณเข้าไป
                            var h = v * (height * 0.48) * canvas.amplitudeGain
                            ctx.fillRect(x, mid-h, 1, h*2)
                        }

                        // ===== playhead =====
                        var dur = activeDurationMs()
                        var phx = dur > 0 ? contentPxToScreenX(sampleToPx(msToSample(playPosMs))) : 0
                        ctx.strokeStyle = playheadColor
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        ctx.moveTo(phx, 0)
                        ctx.lineTo(phx, height)
                        ctx.stroke()
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton

                        property bool draggingPlayhead: false

                        onWheel: {
                            var factor = (wheel.angleDelta.y > 0) ? 1.15 : (1/1.15)
                            zoomAt(mouse.x, factor)
                        }
                        onPressed: {
                            lastMouseX = mouse.x
                            var sIdx = screenXToSample(mouse.x)
                            var phx = contentPxToScreenX(sampleToPx(msToSample(playPosMs)))
                            if (Math.abs(mouse.x - phx) < 8) {
                                draggingPlayhead = true
                                player.pause()
                            } else if (mouse.button === Qt.LeftButton) {
                                isSelecting = true
                                selStart = selEnd = sIdx
                                seekToSample(selStart)
                            } else {
                                isDraggingPane = true
                            }
                        }
                        onPositionChanged: {
                            if (draggingPlayhead) {
                                var sIdx = screenXToSample(mouse.x)
                                seekToSample(sIdx)
                            } else if (isDraggingPane) {
                                var dx = mouse.x - lastMouseX
                                scrollPx = clamp(scrollPx - dx, 0, Math.max(0, contentWidthPx()-canvas.width))
                                lastMouseX = mouse.x
                                canvas.requestPaint()
                            } else if (isSelecting) {
                                selEnd = screenXToSample(mouse.x)
                                canvas.requestPaint()
                            }
                        }
                        onReleased: {
                            if (draggingPlayhead) {
                                draggingPlayhead = false
                                if (enginePlaying) player.play()
                            } else if (isSelecting) {
                                if (selEnd < selStart) { var t=selStart; selStart=selEnd; selEnd=t }
                                isSelecting=false
                                canvas.requestPaint()
                            }
                            isDraggingPane=false
                        }

                        onDoubleClicked: {
                            if (selStart>=0 && selEnd>selStart)
                                fitToSelection()
                            else
                                fitZoom()
                        }
                    }

                    // ป้ายเวลา Now (อิง playPosMs/total)
                    Item {
                        id: waveForm
                        anchors.fill: parent
                        visible: (durationMs > 0)
                        property real phx: {
                            var s = msToSample(playPosMs)
                            return contentPxToScreenX(sampleToPx(s))
                        }
                        x: phx - timeBg.width/2
                        y: 4

                        Rectangle {
                            id: timeBg
                            radius: 6
                            color: isDarkTheme ? "#1f2937" : "#ffffff"
                            border.color: isDarkTheme ? "#0b0e12" : "#e5e7eb"
                            anchors.horizontalCenter: parent.horizontalCenter

                            Text {
                                id: timeText
                                anchors.margins: 6
                                anchors.centerIn: parent
                                color: isDarkTheme ? "#e5e7eb" : "#111827"
                                font.pixelSize: 12
                                text: {
                                    var totalSec = (durationMs > 0 ? durationMs/1000.0 : 0)
                                    var nowSec   = Math.max(0, Math.min(totalSec, playPosMs/1000.0))
                                    fmtDuration(nowSec)
                                }
                            }

                            width: timeText.paintedWidth + 12
                            height: timeText.paintedHeight + 6
                        }
                    }
                }
            }
        }
    }
//    Component.onCompleted: {
//        var st = fileReader.loadWaveSelectionState()
//        if (st && st.ok && st.files && st.files.length > 0) {
//            setFiles(st.files, st.summary)   // ✅ ส่ง summary เข้าไป
//            console.log("[RESTORE] summary:", JSON.stringify(st.summary))
//        } else {
//            console.log("[RESTORE] no saved selection")
//        }
//    }
    Component.onCompleted: {
        // ✅ เริ่ม restore
        restoringSelection = true
//        console.log("[RESTORE] start -> restoringSelection=true")

        var st = fileReader.loadWaveSelectionState()
        if (st && st.ok && st.files && st.files.length > 0) {
            setFiles(st.files, st.summary)
//            console.log("[RESTORE] summary:", JSON.stringify(st.summary))
        } else {
//            console.log("[RESTORE] no saved selection")
        }

        // ✅ จบ restore
        restoringSelection = false
//        console.log("[RESTORE] done -> restoringSelection=false")
    }

}
