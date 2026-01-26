// MapViewer.qml  (FULL FILE)
// ✅ MODE B: DOA **NOT** depend on headingDeg at all
// ✅ DOA/world = doaDeg (+offset) ONLY
// ✅ thetaArray rotate = thetaDeg (+offset) ONLY
// ✅ Map rotation still works because canvases already rotate by -map.bearing

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtLocation 5.15
import QtPositioning 5.15
import QtQuick.Window 2.15
import QtGraphicalEffects 1.15
import "./"

Item {
    id: mapviewer
    width: 1920
    height: 1080

    signal requestScreenshot()

    // ================== STYLE CONFIG (3 styles) ==================
    property int styleIndex: 2
    property string darkStyleUrl:  "http://192.168.10.13:8080/styles/googlemapbright/style.json"
    property string lightStyleUrl: "http://127.0.0.1:8080/styles/osm-bright-ifz/style.json"
    property string thirdStyleUrl: "http://192.168.10.13:8080/styles/satellite-hybrid/style.json"

    property string currentStyleUrl: (styleIndex === 0) ? darkStyleUrl
                                   : (styleIndex === 1) ? lightStyleUrl
                                   : thirdStyleUrl
    property bool isDarkTheme: (styleIndex === 0)

    property real oldLat: 0
    property real oldLon: 0
    property real oldZoom: 13
    property real oldBearing: 0

    property bool compassVisible: false
    property bool navigationEnabled: false
    property bool followPositionEnabled: false

    // ✅ IMPORTANT: default ให้โชว์
    property bool viewerVisible: true

    property bool needScreenshot: false
    property var doaChannelVisibleBackup: []

    // ================== MULTI GPS PIN MODEL ==================
    ListModel { id: gpsPinsModel }

    // ================== MULTI DOA MODEL ==================
    ListModel { id: doaPinsModel }

    // ====== Pin timeout / cleanup ======
    property int pinTimeoutMs: 2000        // 2 วิ
    property int cleanupIntervalMs: 1000   // ตรวจทุก 1 วิ

    // DOA timeout
    property int doaTimeoutMs: 8000

    // tick สำหรับบังคับ repaint DOA delegate
    property int doaUpdateTick: 0

    // tick สำหรับบังคับ repaint compass/pin ตาม device
    property int compassTick: 0

    // ✅ reactive clock
    property int doaNowMs: 0

    // pending DOA ถ้ามาก่อน GPS
    property var pendingDoaFrames: ({})

    // ✅ pending MAX-ONLY ถ้ามาก่อน GPS
    property var pendingMaxDoaFrames: ({})

    // selected key
    property string selectedKey: ""
    property var selectedCoord: QtPositioning.coordinate(13.75, 100.5)

    // =====================================================================
    // DOA REFERENCE (IGNORE headingDeg completely)
    // =====================================================================
    property bool doaFlipSign: false
    property real doaOffsetDeg: 0

    function wrap360(deg) {
        deg = Number(deg)
        if (!isFinite(deg)) return 0
        deg = deg % 360
        if (deg < 0) deg += 360
        return deg
    }

    // ✅ NOT using headingDeg
    function worldDoaNoHeading(rawDoaDeg, offsetDeg) {
        var d = Number(rawDoaDeg)
        var off = Number(offsetDeg || 0)
        if (!isFinite(d)) d = 0
        if (!isFinite(off)) off = 0
        if (mapviewer.doaFlipSign) d = -d
        return wrap360(d + off)
    }

    // ✅ thetaArray rotate WITHOUT headingDeg
    function rotateThetaArrayToWorldNoHeading(thetaArray, offsetDeg) {
        if (!thetaArray) return []
        var off = Number(offsetDeg || 0)
        if (!isFinite(off)) off = 0

        var out = new Array(thetaArray.length)
        for (var i = 0; i < thetaArray.length; ++i) {
            var th = Number(thetaArray[i])
            if (!isFinite(th)) th = 0
            if (mapviewer.doaFlipSign) th = -th
            out[i] = wrap360(th + off)
        }
        return out
    }

    // ================== KEY NORMALIZATION ==================
    function norm(s) {
        s = (s !== undefined && s !== null) ? String(s) : ""
        s = s.trim()
        s = s.replace(/\s+/g, " ")
        return s
    }

    function keyOf(serial, name) {
        serial = norm(serial)
        name   = norm(name)
        if (serial.length === 0) serial = name.length ? name : "UNKNOWN"
        if (name.length === 0)   name   = "NONAME"
        return serial + "|" + name
    }

    function coordOfKey(key) {
        for (var i=0; i<gpsPinsModel.count; ++i) {
            var it = gpsPinsModel.get(i)
            if (it.key === key)
                return QtPositioning.coordinate(it.lat, it.lon)
        }
        return null
    }

    function findGpsIndexByKey(key) {
        for (var i=0; i<gpsPinsModel.count; ++i) {
            if (gpsPinsModel.get(i).key === key) return i
        }
        return -1
    }

    function findDoaIndexByKey(key) {
        for (var i=0; i<doaPinsModel.count; ++i) {
            if (doaPinsModel.get(i).key === key) return i
        }
        return -1
    }

    function removeDoaByKey(key) {
        var idx = findDoaIndexByKey(key)
        if (idx >= 0) doaPinsModel.remove(idx)
        if (pendingDoaFrames && pendingDoaFrames[key] !== undefined) delete pendingDoaFrames[key]
        if (pendingMaxDoaFrames && pendingMaxDoaFrames[key] !== undefined) delete pendingMaxDoaFrames[key]
    }

    // ===== FULL-FRAME helper =====
    function isFullDoaItem(it) {
        if (!it) return false
        var tj = String(it.thetaJson || "[]")
        var rj = String(it.rJson || "[]")
        if (tj.length < 4 || rj.length < 4) return false
        try {
            var th = JSON.parse(tj)
            var rr = JSON.parse(rj)
            return th && rr && th.length >= 2 && rr.length >= 2
        } catch(e) {
            return false
        }
    }

    function hasAnyActiveDoa() {
        var now = mapviewer.doaNowMs
        for (var i = 0; i < doaPinsModel.count; ++i) {
            var it = doaPinsModel.get(i)
            if (now - (it.updatedMs || 0) > mapviewer.doaTimeoutMs) continue
            if (isFullDoaItem(it)) return true
        }
        return false
    }

    function hasAnyActiveMaxDoa() {
        var now = mapviewer.doaNowMs
        for (var i = 0; i < doaPinsModel.count; ++i) {
            var it = doaPinsModel.get(i)
            if ((now - (it.updatedMs || 0)) > mapviewer.doaTimeoutMs) continue
            var deg = Number(it.doaDeg)
            if (!isFinite(deg)) continue
            return true
        }
        return false
    }

    function isDoaActive(it) {
        var now = mapviewer.doaNowMs
        return (now - (it.updatedMs || 0) <= mapviewer.doaTimeoutMs)
    }

    function isValidDoaFrame(thetaArray, spectrumArray, doaDeg, confidence) {
        const deg  = Number(doaDeg)
        const conf = Number(confidence)
        if (!isFinite(deg) || !isFinite(conf)) return false
        if (conf < 0.1) return false
        const thOk = thetaArray && thetaArray.length >= 2
        const spOk = spectrumArray && spectrumArray.length >= 2
        if (!thOk || !spOk) return false
        return true
    }

    function isValidMaxOnly(doaDeg, confidence) {
        const deg  = Number(doaDeg)
        const conf = Number(confidence)
        if (!isFinite(deg) || !isFinite(conf)) return false
        if (conf < 0.1) return false
        return true
    }

    function _kickRepaintAllDoa() {
        Qt.callLater(function() { mapviewer.doaUpdateTick++ })
    }

    // ================== COMPASS UPDATE (keep for UI only) ==================
    function upsertCompass(serial, name, headingDeg) {
        var k = keyOf(serial, name)
        var h = Number(headingDeg)
        if (!isFinite(h)) return

        // update gps pin heading (แค่โชว์หัวเข็ม/เส้น heading เท่านั้น)
        var gi = findGpsIndexByKey(k)
        if (gi >= 0) {
            var it = gpsPinsModel.get(gi)
            gpsPinsModel.set(gi, {
                key: it.key,
                serial: it.serial,
                name: it.name,
                lat: it.lat, lon: it.lon, alt: it.alt,
                dateStr: it.dateStr,
                timeStr: it.timeStr,
                updatedMs: it.updatedMs,
                r: it.r, g: it.g, b: it.b,

                headingDeg: h,
                headingUpdatedMs: Date.now()
            })
        }

        // update doa pin heading (เก็บไว้เฉยๆ ไม่เอาไป rotate DOA)
        var di = findDoaIndexByKey(k)
        if (di >= 0) {
            var d = doaPinsModel.get(di)
            doaPinsModel.set(di, {
                key: d.key,
                lat: d.lat, lon: d.lon,
                updatedMs: d.updatedMs,
                firstSeenMs: d.firstSeenMs,

                thetaRawJson: d.thetaRawJson,
                thetaJson: d.thetaJson,
                rJson: d.rJson,

                doaDeg: d.doaDeg,
                doaRawDeg: d.doaRawDeg,
                confidence: d.confidence,

                headingDeg: h
            })
        }

        compassTick++
        if (mapLoader.item && mapLoader.item.map) {
            if (mapLoader.item.headingLineCanvas) mapLoader.item.headingLineCanvas.safeRequestPaint()
            if (mapLoader.item.maxDoaLineCanvas)  mapLoader.item.maxDoaLineCanvas.safeRequestPaint()
        }
    }

    // ================== DOA UPSERT (NO heading) ==================
    function upsertDoaFrame(serial, controllerName, thetaArray, spectrumArray, doaDeg, confidence, headingDegIgnored) {
        var k = keyOf(serial, controllerName)

        if (!isValidDoaFrame(thetaArray, spectrumArray, doaDeg, confidence)) {
            removeDoaByKey(k)
            _kickRepaintAllDoa()
            return
        }

        var c = coordOfKey(k)
        if (!c) {
            pendingDoaFrames[k] = {
                thetaArr: thetaArray || [],
                rArr: spectrumArray || [],
                doaDeg: Number(doaDeg),
                confidence: Number(confidence),
                headingDeg: Number(headingDegIgnored),
                updatedMs: Date.now()
            }
            return
        }

        var doaWorldDeg   = worldDoaNoHeading(doaDeg, mapviewer.doaOffsetDeg)
        var thetaWorldArr = rotateThetaArrayToWorldNoHeading(thetaArray || [], mapviewer.doaOffsetDeg)

        var idx = findDoaIndexByKey(k)
        var firstSeen = Date.now()
        if (idx >= 0) {
            var prev = doaPinsModel.get(idx)
            firstSeen = Number(prev.firstSeenMs || prev.updatedMs || Date.now())
        }

        var obj = {
            key: k,
            lat: c.latitude,
            lon: c.longitude,
            updatedMs: Date.now(),
            firstSeenMs: firstSeen,

            thetaRawJson: JSON.stringify(thetaArray || []),
            thetaJson:    JSON.stringify(thetaWorldArr),
            rJson:        JSON.stringify(spectrumArray || []),

            doaDeg: doaWorldDeg,
            doaRawDeg: Number(doaDeg),

            confidence: Number(confidence),
            headingDeg: Number(headingDegIgnored || 0) // เก็บไว้เฉยๆ
        }

        if (idx >= 0) doaPinsModel.set(idx, obj)
        else doaPinsModel.append(obj)
    }

    function upsertMaxOnlyDoa(serial, controllerName, doaDeg, confidence, headingDegIgnored) {
        var k = keyOf(serial, controllerName)

        if (!isValidMaxOnly(doaDeg, confidence)) {
            removeDoaByKey(k)
            _kickRepaintAllDoa()
            return
        }

        var c = coordOfKey(k)
        if (!c) {
            pendingMaxDoaFrames[k] = {
                doaDeg: Number(doaDeg),
                confidence: Number(confidence),
                headingDeg: Number(headingDegIgnored),
                updatedMs: Date.now()
            }
            return
        }

        var doaWorldDeg = worldDoaNoHeading(doaDeg, mapviewer.doaOffsetDeg)

        var idx = findDoaIndexByKey(k)
        var firstSeen = Date.now()
        if (idx >= 0) {
            var prev = doaPinsModel.get(idx)
            firstSeen = Number(prev.firstSeenMs || prev.updatedMs || Date.now())
        }

        var obj = {
            key: k,
            lat: c.latitude,
            lon: c.longitude,
            updatedMs: Date.now(),
            firstSeenMs: firstSeen,

            thetaRawJson: "[]",
            thetaJson: "[]",
            rJson: "[]",

            doaDeg: doaWorldDeg,
            doaRawDeg: Number(doaDeg),
            confidence: Number(confidence),
            headingDeg: Number(headingDegIgnored || 0)
        }

        if (idx >= 0) doaPinsModel.set(idx, obj)
        else doaPinsModel.append(obj)
    }

    // ================== Unique random color pool (no duplicates) ==================
    property var usedColorKeys: ({})

    function _randInt(min, max) { return Math.floor(min + Math.random() * (max - min + 1)) }
    function _clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)) }
    function _rgbKey(r,g,b) { return r + "," + g + "," + b }

    function _hslToRgb(h, s, l) {
        h = (h % 360 + 360) % 360
        var c = (1 - Math.abs(2*l - 1)) * s
        var x = c * (1 - Math.abs(((h/60) % 2) - 1))
        var m = l - c/2

        var r1=0, g1=0, b1=0
        if (0 <= h && h < 60)         { r1=c; g1=x; b1=0 }
        else if (60 <= h && h < 120)  { r1=x; g1=c; b1=0 }
        else if (120 <= h && h < 180) { r1=0; g1=c; b1=x }
        else if (180 <= h && h < 240) { r1=0; g1=x; b1=c }
        else if (240 <= h && h < 300) { r1=x; g1=0; b1=c }
        else                          { r1=c; g1=0; b1=x }

        var r = Math.round((r1 + m) * 255)
        var g = Math.round((g1 + m) * 255)
        var b = Math.round((b1 + m) * 255)
        r = _clamp(r, 0, 255); g=_clamp(g,0,255); b=_clamp(b,0,255)
        return { r:r, g:g, b:b }
    }

    function pickUniqueRandomColor() {
        for (var tries=0; tries<2000; ++tries) {
            var h = _randInt(0, 359)
            var rgb = _hslToRgb(h, 0.90, 0.55)
            var key = _rgbKey(rgb.r, rgb.g, rgb.b)
            if (!usedColorKeys[key]) {
                usedColorKeys[key] = true
                return rgb
            }
        }
        var rr = _randInt(40, 255)
        var gg = _randInt(40, 255)
        var bb = _randInt(40, 255)
        var k2 = _rgbKey(rr,gg,bb)
        if (!usedColorKeys[k2]) usedColorKeys[k2] = true
        return {r:rr,g:gg,b:bb}
    }

    function upsertGpsPin(serial, name, lat, lon, alt, dateStr, timeStr) {
        serial = norm(serial)
        name   = norm(name)

        var key = keyOf(serial, name)

        lat = Number(lat); lon = Number(lon); alt = Number(alt)
        if (!isFinite(lat) || !isFinite(lon)) return
        if (!isFinite(alt)) alt = 0

        var found = -1
        for (var i = 0; i < gpsPinsModel.count; ++i) {
            if (gpsPinsModel.get(i).key === key) { found = i; break }
        }

        if (found >= 0) {
            var old = gpsPinsModel.get(found)
            gpsPinsModel.set(found, {
                key: key,
                serial: serial,
                name: name,
                lat: lat, lon: lon, alt: alt,
                dateStr: String(dateStr || ""),
                timeStr: String(timeStr || ""),
                updatedMs: Date.now(),
                r: old.r, g: old.g, b: old.b,

                headingDeg: old.headingDeg,
                headingUpdatedMs: old.headingUpdatedMs
            })

            if (selectedKey === key) selectedCoord = QtPositioning.coordinate(lat, lon)

            var didx = findDoaIndexByKey(key)
            if (didx >= 0) {
                var itDoa = doaPinsModel.get(didx)
                doaPinsModel.set(didx, {
                    key: itDoa.key,
                    lat: lat, lon: lon,
                    updatedMs: itDoa.updatedMs,
                    firstSeenMs: itDoa.firstSeenMs,

                    thetaRawJson: itDoa.thetaRawJson,
                    thetaJson: itDoa.thetaJson,
                    rJson: itDoa.rJson,

                    doaDeg: itDoa.doaDeg,
                    doaRawDeg: itDoa.doaRawDeg,
                    confidence: itDoa.confidence,
                    headingDeg: itDoa.headingDeg
                })
            }
        } else {
            var rgb = pickUniqueRandomColor()
            gpsPinsModel.append({
                key: key,
                serial: serial,
                name: name,
                lat: lat, lon: lon, alt: alt,
                dateStr: String(dateStr || ""),
                timeStr: String(timeStr || ""),
                updatedMs: Date.now(),
                r: rgb.r, g: rgb.g, b: rgb.b,

                headingDeg: 0,
                headingUpdatedMs: 0
            })

            if (selectedKey.length === 0) {
                selectedKey = key
                selectedCoord = QtPositioning.coordinate(lat, lon)
            }
        }

        var pend = pendingDoaFrames[key]
        if (pend) {
            upsertDoaFrame(serial, name, pend.thetaArr, pend.rArr, pend.doaDeg, pend.confidence, pend.headingDeg)
            delete pendingDoaFrames[key]
            _kickRepaintAllDoa()
        }

        var pendMax = pendingMaxDoaFrames[key]
        if (pendMax) {
            upsertMaxOnlyDoa(serial, name, pendMax.doaDeg, pendMax.confidence, pendMax.headingDeg)
            delete pendingMaxDoaFrames[key]
            _kickRepaintAllDoa()
        }
    }

    // =====================================================================
    // TX ESTIMATE (unchanged)
    // =====================================================================
    property bool txVisible: true
    property var  txEstimate: ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0 })
    property int  txHistoryMax: 25
    ListModel { id: txHistoryModel }

    property real intersectionMaxResidualM: 250
    property real txSnapDistanceM: 30

    function _deg2rad(d){ return Number(d) * Math.PI / 180.0 }
    function _rad2deg(r){ return Number(r) * 180.0 / Math.PI }

    function _haversineMeters(lat1, lon1, lat2, lon2) {
        const R = 6378137.0
        const p1 = _deg2rad(lat1)
        const p2 = _deg2rad(lat2)
        const dP = _deg2rad(lat2 - lat1)
        const dL = _deg2rad(lon2 - lon1)

        const a = Math.sin(dP/2)*Math.sin(dP/2) +
                  Math.cos(p1)*Math.cos(p2) * Math.sin(dL/2)*Math.sin(dL/2)
        const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a))
        return R * c
    }

    function _latLonToXY_m(lat, lon, refLat, refLon) {
        const R = 6378137.0
        const dLat = _deg2rad(lat - refLat)
        const dLon = _deg2rad(lon - refLon)
        const cos0 = Math.cos(_deg2rad(refLat))
        return { x: R * dLon * cos0, y: R * dLat }
    }

    function _xyToLatLon(x, y, refLat, refLon) {
        const R = 6378137.0
        const lat = refLat + _rad2deg(y / R)
        const lon = refLon + _rad2deg(x / (R * Math.cos(_deg2rad(refLat))))
        return { lat: lat, lon: lon }
    }

    function _bearingToUnitENU(bearingDeg) {
        const br = _deg2rad(bearingDeg)
        return { ux: Math.sin(br), uy: Math.cos(br) }
    }

    function _intersect2Lines(p1, u1, p2, u2) {
        const det = u1.ux * u2.uy - u1.uy * u2.ux
        if (Math.abs(det) < 1e-9) return { ok:false }

        const dx = p2.x - p1.x
        const dy = p2.y - p1.y

        const t = ( dx * u2.uy - dy * u2.ux) / det
        const s = ( dx * u1.uy - dy * u1.ux) / det

        return { ok:true, x: p1.x + t*u1.ux, y: p1.y + t*u1.uy, t:t, s:s }
    }

    function _bestPointForLines(points, units) {
        const n = Math.min(points.length, units.length)
        if (n < 2) return { ok:false }

        let A11=0, A12=0, A22=0
        let b1=0,  b2=0

        for (let i=0; i<n; ++i) {
            const ux = units[i].ux, uy = units[i].uy
            const p  = points[i]

            const m11 = 1 - ux*ux
            const m12 =   - ux*uy
            const m22 = 1 - uy*uy

            A11 += m11
            A12 += m12
            A22 += m22

            b1 += m11*p.x + m12*p.y
            b2 += m12*p.x + m22*p.y
        }

        const det = A11*A22 - A12*A12
        if (Math.abs(det) < 1e-9) return { ok:false }

        const x = ( b1*A22 - b2*A12 ) / det
        const y = ( A11*b2 - A12*b1 ) / det

        let sum2 = 0
        for (let i=0; i<n; ++i) {
            const ux = units[i].ux, uy = units[i].uy
            const p  = points[i]
            const dx = x - p.x
            const dy = y - p.y
            const perp = dx*(-uy) + dy*(ux)
            sum2 += perp*perp
        }
        const rms = Math.sqrt(sum2 / n)
        return { ok:true, x:x, y:y, rms:rms }
    }

    function rebuildTxEstimateKrakenLike() {
        if (!txVisible) {
            txEstimate = ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0 })
            txHistoryModel.clear()
            return
        }

        let items = []
        for (let i=0; i<doaPinsModel.count; ++i) {
            const it = doaPinsModel.get(i)
            if (!mapviewer.isDoaActive(it)) continue

            const lat = Number(it.lat), lon = Number(it.lon)
            let deg = Number(it.doaDeg)
            if (!isFinite(lat) || !isFinite(lon) || !isFinite(deg)) continue
            deg = mapviewer.wrap360(deg)
            items.push({ lat:lat, lon:lon, deg:deg, key:it.key })
        }

        if (items.length < 2) {
            txEstimate = ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0 })
            return
        }

        let refLat=0, refLon=0
        for (let i=0; i<items.length; ++i) { refLat += items[i].lat; refLon += items[i].lon }
        refLat /= items.length; refLon /= items.length

        let pts = []
        let uvs = []
        for (let i=0; i<items.length; ++i) {
            pts.push(_latLonToXY_m(items[i].lat, items[i].lon, refLat, refLon))
            uvs.push(_bearingToUnitENU(items[i].deg))
        }

        let x=0, y=0, rms=0

        if (items.length === 2) {
            const r = _intersect2Lines(pts[0], uvs[0], pts[1], uvs[1])
            if (!r.ok || r.t < 0 || r.s < 0) {
                txEstimate = ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0 })
                return
            }
            x = r.x; y = r.y; rms = 0
        } else {
            const best = _bestPointForLines(pts, uvs)
            if (!best.ok || best.rms > mapviewer.intersectionMaxResidualM) {
                txEstimate = ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0 })
                return
            }
            x = best.x; y = best.y; rms = best.rms
        }

        const ll = _xyToLatLon(x, y, refLat, refLon)
        let est = ({
            valid: true,
            lat: ll.lat,
            lon: ll.lon,
            rms: rms,
            count: items.length,
            updatedMs: Date.now()
        })

        let snapped = false
        if (mapviewer.txEstimate.valid) {
            const d0 = _haversineMeters(est.lat, est.lon, mapviewer.txEstimate.lat, mapviewer.txEstimate.lon)
            if (d0 <= mapviewer.txSnapDistanceM) {
                est.lat = mapviewer.txEstimate.lat
                est.lon = mapviewer.txEstimate.lon
                snapped = true
            }
        }

        if (!snapped && txHistoryModel.count > 0) {
            const h0 = txHistoryModel.get(0)
            const d1 = _haversineMeters(est.lat, est.lon, h0.lat, h0.lon)
            if (d1 <= mapviewer.txSnapDistanceM) {
                est.lat = h0.lat
                est.lon = h0.lon
                snapped = true
            }
        }

        txEstimate = est

        if (txHistoryModel.count === 0) {
            txHistoryModel.insert(0, { lat: est.lat, lon: est.lon, rms: est.rms, updatedMs: est.updatedMs })
        } else {
            const h = txHistoryModel.get(0)
            const d2 = _haversineMeters(est.lat, est.lon, h.lat, h.lon)

            if (d2 <= mapviewer.txSnapDistanceM) {
                txHistoryModel.set(0, { lat: h.lat, lon: h.lon, rms: est.rms, updatedMs: est.updatedMs })
            } else {
                txHistoryModel.insert(0, { lat: est.lat, lon: est.lon, rms: est.rms, updatedMs: est.updatedMs })
                while (txHistoryModel.count > txHistoryMax)
                    txHistoryModel.remove(txHistoryModel.count - 1)
            }
        }
    }

    function updateMaxDoaMonitor() {
        if (!viewerHud || !viewerHud.logger) return

        var best = null
        var bestConf = -1
        for (var i = 0; i < doaPinsModel.count; ++i) {
            var it = doaPinsModel.get(i)
            if (!mapviewer.isDoaActive(it)) continue
            var conf = Number(it.confidence || 0)
            if (!isFinite(conf)) conf = 0
            if (conf > bestConf) { bestConf = conf; best = it }
        }
        if (!best) return
        var ageMs = Date.now() - Number(best.firstSeenMs || best.updatedMs || 0)
        if (!isFinite(ageMs) || ageMs < 3000) return

        viewerHud.logger.feedMaxDoaCandidate({
            key: String(best.key || ""),
            doa: Number(best.doaDeg || 0),
            confidence: Number(best.confidence || 0),
            heading: Number(best.headingDeg || 0),
            lat: Number(best.lat || 0),
            lon: Number(best.lon || 0),
        })
    }

    Timer {
        id: doaNowTimer
        interval: 250
        running: true
        repeat: true
        onTriggered: {
            mapviewer.doaNowMs = Date.now()
            mapviewer.doaUpdateTick++

            mapviewer.rebuildTxEstimateKrakenLike()
            mapviewer.updateMaxDoaMonitor()

            if (mapLoader.item) {
                if (mapLoader.item.headingLineCanvas) mapLoader.item.headingLineCanvas.safeRequestPaint()
                if (mapLoader.item.maxDoaLineCanvas)  mapLoader.item.maxDoaLineCanvas.safeRequestPaint()
            }
        }
    }

    Timer {
        id: cleanupTimer
        interval: mapviewer.cleanupIntervalMs
        running: true
        repeat: true
        onTriggered: {
            var now = Date.now()

            for (var i = gpsPinsModel.count - 1; i >= 0; --i) {
                var it = gpsPinsModel.get(i)
                var age = now - (it.updatedMs || 0)
                if (age > mapviewer.pinTimeoutMs) {
                    var removedKey = it.key
                    gpsPinsModel.remove(i)

                    if (mapviewer.selectedKey === removedKey) {
                        if (gpsPinsModel.count > 0) {
                            var j = Math.min(i, gpsPinsModel.count - 1)
                            var it2 = gpsPinsModel.get(j)
                            mapviewer.selectedKey = it2.key
                            mapviewer.selectedCoord = QtPositioning.coordinate(it2.lat, it2.lon)
                        } else {
                            mapviewer.selectedKey = ""
                            mapviewer.selectedCoord = QtPositioning.coordinate(13.75, 100.5)
                        }
                    }

                    mapviewer.removeDoaByKey(removedKey)
                }
            }

            var now2 = Date.now()
            for (var d = doaPinsModel.count - 1; d >= 0; --d) {
                var dit = doaPinsModel.get(d)
                var dage = now2 - (dit.updatedMs || 0)
                if (dage > mapviewer.doaTimeoutMs) {
                    doaPinsModel.remove(d)
                }
            }

            if (!mapviewer.hasAnyActiveMaxDoa()) {
                mapviewer.txEstimate = ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0 })
                txHistoryModel.clear()
            }
        }
    }

    // ================== MAP LOADER ==================
    Loader {
        id: mapLoader
        anchors.fill: parent
        sourceComponent: mapComponent
    }

    // ============================================================
    // DoaHistoryViewer HUD Overlay
    // ============================================================
    DoaHistoryViewer {
        id: viewerHud
        visible: mapviewer.viewerVisible
        z: 99999
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 350
        anchors.rightMargin: 30

        txModel: txHistoryModel
        rfCache: rfcache
    }

    Connections {
        target: Krakenmapval
        function onUpdateLocalTime(currentTime, currentDate, uptime) {
            viewerHud.logger.saveTime(currentTime, currentDate, uptime)
        }
    }

    QtObject {
        id: rfcache
        property real lastRfFreqHz: 0
        property real lastRfBwHz: 0
        function save(freqHz, bwHz){
            var f = Number(freqHz)
            var b = Number(bwHz)
            if (isFinite(f) && f > 0) lastRfFreqHz = f
            if (isFinite(b) && b > 0) lastRfBwHz   = b
        }
    }
    Connections {
        target: Krakenmapval
        function onRfsocParameterUpdated(freqHz, bwHz) {
            rfcache.save(freqHz, bwHz)
        }
    }

    // toggle viewer button (png)
    Item {
        id: viewerToggleBtn
        width: 48
        height: 48
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.bottomMargin: 290
        anchors.rightMargin: 30
        z: 100000

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: mapviewer.viewerVisible ? "#2ecc71" : "#C7C8CC"
            opacity: 0.7
            MouseArea { anchors.fill: parent; onClicked: mapviewer.viewerVisible = !mapviewer.viewerVisible }
        }

        Image {
            anchors.centerIn: parent
            width: 28
            height: 28
            fillMode: Image.PreserveAspectFit
            smooth: true
            source: mapviewer.viewerVisible
                    ? "qrc:/iScreenDFqml/images/logger_on.png"
                    : "qrc:/iScreenDFqml/images/logger_off.png"
        }
    }

    // ================== MAP COMPONENT ==================
    Component {
        id: mapComponent

        Item {
            id: mapWrapper
            anchors.fill: parent
            property alias map: map

            property alias headingLineCanvas: headingLineCanvas
            property alias maxDoaLineCanvas:  maxDoaLineCanvas

            Plugin {
                id: mapboxPlugin
                name: "mapboxgl"
                PluginParameter { name: "mapboxgl.access_token"; value: "no-token-required" }
                PluginParameter { name: "mapboxgl.mapping.cache.memory"; value: "10" }
                PluginParameter { name: "mapboxgl.mapping.cache.size"; value: "10485760" }
                PluginParameter { name: "mapboxgl.mapping.transitions.fadeDuration"; value: "0" }
                PluginParameter { name: "mapboxgl.mapping.tilecache.enable"; value: "false" }
                PluginParameter { name: "mapboxgl.mapping.cache.database"; value: "off" }
                PluginParameter { name: "mapboxgl.mapping.offline_mode"; value: "true" }
                PluginParameter { name: "mapboxgl.mapping.additional_style_urls"; value: mapviewer.currentStyleUrl }
            }

            Map {
                id: map
                anchors.fill: parent
                plugin: mapboxPlugin
                center: QtPositioning.coordinate(13.75, 100.5)
                zoomLevel: 13
                maximumZoomLevel: 17

                Behavior on bearing {
                    NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
                }

                Connections {
                    target: Krakenmapval
                    function onUpdateGpsMarker(serial, controllerName, lat, lon, alt, dateStr, timeStr) {
                        mapviewer.upsertGpsPin(serial, controllerName, lat, lon, alt, dateStr, timeStr)
                        if (mapviewer.followPositionEnabled) map.center = mapviewer.selectedCoord
                    }
                }

                Connections {
                    target: Krakenmapval
                    function onUpdateDegree(serial, name, heading) {
                        mapviewer.upsertCompass(mapviewer.norm(serial), mapviewer.norm(name), heading)
                    }
                }

                Connections {
                    target: Krakenmapval
                    function onDoaFrameUpdated(Serialnumber, controllerName, thetaArray, spectrumArray, doaDeg, confidence) {
                        var s = mapviewer.norm(Serialnumber)
                        var n = mapviewer.norm(controllerName)

                        var thOk = thetaArray && thetaArray.length >= 2
                        var spOk = spectrumArray && spectrumArray.length >= 2

                        // ✅ headingNow still fetched but NOT used for DOA conversion
                        var k = mapviewer.keyOf(s, n)
                        var headingNow = 0
                        var gi = mapviewer.findGpsIndexByKey(k)
                        if (gi >= 0) headingNow = Number(gpsPinsModel.get(gi).headingDeg || 0)

                        if (thOk && spOk) mapviewer.upsertDoaFrame(s, n, thetaArray, spectrumArray, doaDeg, confidence, headingNow)
                        else              mapviewer.upsertMaxOnlyDoa(s, n, doaDeg, confidence, headingNow)

                        Qt.callLater(function() {
                            mapviewer.doaUpdateTick++
                            mapviewer.rebuildTxEstimateKrakenLike()
                            mapviewer.updateMaxDoaMonitor()
                            headingLineCanvas.safeRequestPaint()
                            maxDoaLineCanvas.safeRequestPaint()
                        })
                    }
                }

                Connections {
                    target: doaPinsModel
                    function onCountChanged() {
                        Qt.callLater(function() {
                            mapviewer.doaUpdateTick++
                            mapviewer.rebuildTxEstimateKrakenLike()
                            mapviewer.updateMaxDoaMonitor()
                            headingLineCanvas.safeRequestPaint()
                            maxDoaLineCanvas.safeRequestPaint()
                        })
                    }
                }

                // ================= MULTI PIN GPS =================
                MapItemView {
                    id: gpsPinsView
                    model: gpsPinsModel

                    delegate: MapQuickItem {
                        id: onePin
                        z: (mapviewer.selectedKey === model.key) ? 50 : 10
                        coordinate: QtPositioning.coordinate(model.lat, model.lon)

                        anchorPoint.x: pinRoot.width / 2
                        anchorPoint.y: 30

                        sourceItem: Item {
                            id: pinRoot
                            width: Math.max(160, nameText.implicitWidth + 24)
                            height: 110

                            Canvas {
                                id: pinCanvas
                                width: 60
                                height: 60
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.top: parent.top
                                antialiasing: true

                                property real heading: Number(model.headingDeg || 0) - map.bearing
                                property bool isSel: (mapviewer.selectedKey === model.key)

                                function rgba(r, g, b, a) { return "rgba(" + r + "," + g + "," + b + "," + a + ")" }

                                onPaint: {
                                    const ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)

                                    const cx = width / 2
                                    const cy = height / 2
                                    const radius = 8
                                    const directionLength = 28
                                    const wedgeAngle = Math.PI / 3.2

                                    const startAngle = (heading - 90) * Math.PI / 180 - wedgeAngle / 2
                                    const endAngle   = startAngle + wedgeAngle

                                    const r = model.r, g = model.g, b = model.b

                                    function drawGlow(alpha, sizeScale) {
                                        const grad = ctx.createRadialGradient(cx, cy, radius, cx, cy, directionLength * sizeScale)
                                        grad.addColorStop(0.0, rgba(r,g,b,alpha))
                                        grad.addColorStop(0.4, rgba(r,g,b,alpha * 0.85))
                                        grad.addColorStop(0.8, rgba(r,g,b,alpha * 0.4))
                                        grad.addColorStop(1.0, rgba(r,g,b,0))

                                        ctx.beginPath()
                                        ctx.moveTo(cx, cy)
                                        ctx.lineTo(
                                            cx + directionLength * sizeScale * Math.cos(startAngle),
                                            cy + directionLength * sizeScale * Math.sin(startAngle)
                                        )
                                        ctx.arc(cx, cy, directionLength * sizeScale, startAngle, endAngle)
                                        ctx.closePath()
                                        ctx.fillStyle = grad
                                        ctx.fill()
                                    }

                                    drawGlow(0.20, 1.0)
                                    drawGlow(0.12, 1.45)

                                    ctx.beginPath()
                                    ctx.arc(cx, cy, radius, 0, 2 * Math.PI)
                                    ctx.fillStyle = pinCanvas.isSel ? "#FFFFFF" : rgba(r,g,b,1.0)
                                    ctx.fill()

                                    ctx.lineWidth = pinCanvas.isSel ? 4 : 2
                                    ctx.strokeStyle = pinCanvas.isSel ? rgba(r,g,b,1.0) : "white"
                                    ctx.stroke()
                                }

                                Connections {
                                    target: map
                                    function onBearingChanged() {
                                        pinCanvas.heading = Number(model.headingDeg || 0) - map.bearing
                                        pinCanvas.requestPaint()
                                    }
                                    function onZoomLevelChanged() { pinCanvas.requestPaint() }
                                    function onCenterChanged() { pinCanvas.requestPaint() }
                                }
                                Connections {
                                    target: mapviewer
                                    function onSelectedKeyChanged() { pinCanvas.requestPaint() }
                                    function onCompassTickChanged() {
                                        pinCanvas.heading = Number(model.headingDeg || 0) - map.bearing
                                        pinCanvas.requestPaint()
                                    }
                                }
                                Component.onCompleted: requestPaint()
                            }

                            Rectangle {
                                id: nameBg
                                anchors.top: pinCanvas.bottom
                                anchors.topMargin: 6
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: nameText.implicitWidth + 24
                                height: nameText.implicitHeight + 12
                                radius: 12
                                color: "black"
                                opacity: 0.75
                                border.width: 2
                                border.color: Qt.rgba(model.r/255, model.g/255, model.b/255, 1)

                                Text {
                                    id: nameText
                                    anchors.centerIn: parent
                                    text: (model.name && model.name.length) ? model.name : model.serial
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: "white"
                                    wrapMode: Text.NoWrap
                                    elide: Text.ElideNone
                                    renderType: Text.NativeRendering
                                    style: Text.Outline
                                    styleColor: "black"
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    mapviewer.selectedKey = model.key
                                    mapviewer.selectedCoord = QtPositioning.coordinate(model.lat, model.lon)
                                    if (mapviewer.followPositionEnabled) map.center = mapviewer.selectedCoord
                                }
                            }
                        }
                    }
                }

                // ============================================================
                // DOA OVERLAYS (FULL FRAME only)  (uses thetaJson which already ignores heading)
                // ============================================================
                MapItemView {
                    id: doaOverlaysView
                    model: doaPinsModel
                    visible: mapviewer.hasAnyActiveDoa()

                    delegate: MapQuickItem {
                        id: doaOverlayItem
                        z: 0
                        visible: mapviewer.isDoaActive(doaPinsModel.get(index)) && mapviewer.isFullDoaItem(doaPinsModel.get(index))

                        coordinate: QtPositioning.coordinate(lat, lon)
                        anchorPoint.x: doaCanvas.width / 2
                        anchorPoint.y: doaCanvas.height / 2

                        sourceItem: Canvas {
                            id: doaCanvas
                            property real radiusInMeters: 400
                            width: 10
                            height: 10
                            antialiasing: true

                            function updateCanvasSize() {
                                if (!map) return
                                const center      = QtPositioning.coordinate(lat, lon)
                                const earthRadius = 6378137
                                const dLng        = (radiusInMeters / (earthRadius * Math.cos(Math.PI * center.latitude / 180))) * (180 / Math.PI)
                                const offsetCoord = QtPositioning.coordinate(center.latitude, center.longitude + dLng)

                                const p1 = map.fromCoordinate(center, false)
                                const p2 = map.fromCoordinate(offsetCoord, false)
                                const pxRadius = Math.abs(p2.x - p1.x)
                                const pxSize   = Math.max(2, pxRadius * 2)

                                doaCanvas.width  = pxSize
                                doaCanvas.height = pxSize
                                doaOverlayItem.anchorPoint.x = pxSize / 2
                                doaOverlayItem.anchorPoint.y = pxSize / 2
                            }

                            onPaint: {
                                if (!mapviewer.isDoaActive(doaPinsModel.get(index))) return

                                const ctx = getContext("2d")
                                const w = width
                                const h = height
                                ctx.clearRect(0,0,w,h)

                                var frameTheta = []
                                var frameR = []
                                try { frameTheta = JSON.parse(thetaJson || "[]") } catch(e) { frameTheta = [] }
                                try { frameR     = JSON.parse(rJson || "[]") } catch(e) { frameR = [] }
                                if (frameTheta.length < 2 || frameR.length < 2) return

                                const cx = w/2
                                const cy = h/2
                                const radiusPx = Math.min(w,h) * 0.48
                                const N = Math.min(frameTheta.length, frameR.length)

                                function ampToRadius(v) {
                                    let val = Math.max(0, Math.min(1, v))
                                    return radiusPx * val
                                }

                                function thToRadNorth(deg) {
                                    return (Number(deg) - 90) * Math.PI / 180.0
                                }

                                ctx.save()
                                ctx.translate(cx, cy)

                                // ✅ map rotation only
                                ctx.rotate(-map.bearing * Math.PI/180.0)

                                ctx.strokeStyle = "rgba(63,63,70,0.7)"
                                ctx.lineWidth = 1
                                const rings = 4
                                for (let i=1; i<=rings; ++i) {
                                    const rr = radiusPx * i / rings
                                    ctx.beginPath()
                                    ctx.arc(0,0,rr,0,2*Math.PI)
                                    ctx.stroke()
                                }

                                ctx.strokeStyle = "rgba(69,69,79,0.9)"
                                ctx.lineWidth = 1
                                function drawAxis(degA) {
                                    const radA = thToRadNorth(degA)
                                    const x = radiusPx * Math.cos(radA)
                                    const y = radiusPx * Math.sin(radA)
                                    ctx.beginPath()
                                    ctx.moveTo(0,0)
                                    ctx.lineTo(x,y)
                                    ctx.stroke()
                                }
                                drawAxis(0); drawAxis(90); drawAxis(180); drawAxis(270)

                                ctx.strokeStyle = "rgba(79,195,247,0.9)"
                                ctx.lineWidth = 2
                                ctx.beginPath()
                                for (let i=0; i<N; ++i) {
                                    const th  = frameTheta[i]
                                    const val = frameR[i]
                                    const rr  = ampToRadius(val)
                                    const rad = thToRadNorth(th)

                                    const x = rr * Math.cos(rad)
                                    const y = rr * Math.sin(rad)

                                    if (i===0) ctx.moveTo(x,y)
                                    else       ctx.lineTo(x,y)
                                }
                                ctx.closePath()
                                ctx.stroke()

                                ctx.restore()
                            }

                            Connections {
                                target: mapviewer
                                function onDoaUpdateTickChanged() {
                                    doaCanvas.updateCanvasSize()
                                    doaCanvas.requestPaint()
                                }
                            }

                            Connections {
                                target: map
                                function onZoomLevelChanged() { doaCanvas.updateCanvasSize(); doaCanvas.requestPaint() }
                                function onCenterChanged()    { doaCanvas.updateCanvasSize(); doaCanvas.requestPaint() }
                                function onBearingChanged()   { doaCanvas.updateCanvasSize(); doaCanvas.requestPaint() }
                            }

                            Component.onCompleted: {
                                updateCanvasSize()
                                requestPaint()
                            }
                        }
                    }
                }

                // ============ HEADING LINES (OPTIONAL UI, still uses headingDeg) ============
                Canvas {
                    id: headingLineCanvas
                    anchors.fill: parent
                    antialiasing: true
                    visible: mapviewer.hasAnyActiveMaxDoa()

                    onVisibleChanged: { if (!visible) safeRequestPaint() }

                    property bool paintScheduled: false
                    function safeRequestPaint() {
                        if (!paintScheduled) {
                            paintScheduled = true
                            Qt.callLater(function() {
                                headingLineCanvas.requestPaint()
                                paintScheduled = false
                            })
                        }
                    }

                    onPaint: {
                        if (!map) return
                        const ctx = getContext("2d")
                        ctx.clearRect(0,0,width,height)

                        for (var i=0; i<doaPinsModel.count; ++i) {
                            var it = doaPinsModel.get(i)
                            if (!mapviewer.isDoaActive(it)) continue

                            var coord = QtPositioning.coordinate(it.lat, it.lon)
                            var p = map.fromCoordinate(coord, false)

                            const headingDegLocal = Number(it.headingDeg || 0)
                            const headingRad = (headingDegLocal - map.bearing - 90) * Math.PI/180
                            const len = 120

                            const cx = p.x, cy = p.y
                            const hx = cx + len * Math.cos(headingRad)
                            const hy = cy + len * Math.sin(headingRad)

                            ctx.setLineDash([2,3])
                            ctx.beginPath()
                            ctx.moveTo(cx, cy)
                            ctx.lineTo(hx, hy)
                            ctx.strokeStyle = "#81D4FA"
                            ctx.lineWidth = 2
                            ctx.stroke()
                            ctx.setLineDash([])
                        }
                    }

                    Connections {
                        target: map
                        function onZoomLevelChanged() { headingLineCanvas.safeRequestPaint() }
                        function onCenterChanged()    { headingLineCanvas.safeRequestPaint() }
                        function onBearingChanged()   { headingLineCanvas.safeRequestPaint() }
                    }
                    Connections {
                        target: doaPinsModel
                        function onCountChanged() { headingLineCanvas.safeRequestPaint() }
                    }
                    Connections {
                        target: mapviewer
                        function onDoaUpdateTickChanged() { headingLineCanvas.safeRequestPaint() }
                        function onCompassTickChanged() { headingLineCanvas.safeRequestPaint() }
                    }

                    Component.onCompleted: safeRequestPaint()
                }

                // ============ MAX DOA LINES (uses doaDeg which ignores heading) ============
                Canvas {
                    id: maxDoaLineCanvas
                    anchors.fill: parent
                    antialiasing: true
                    visible: mapviewer.hasAnyActiveMaxDoa()

                    onVisibleChanged: { safeRequestPaint() }

                    property bool paintScheduled: false
                    property real lineLengthMeters: 15000

                    function metersToPixelsAt(coord, meters) {
                        if (!map) return 0
                        const R = 6378137
                        const dLng = (meters / (R * Math.cos(Math.PI * coord.latitude / 180))) * (180 / Math.PI)
                        const p1 = map.fromCoordinate(coord, false)
                        const p2 = map.fromCoordinate(QtPositioning.coordinate(coord.latitude, coord.longitude + dLng), false)
                        return Math.abs(p2.x - p1.x)
                    }

                    function safeRequestPaint() {
                        if (!paintScheduled) {
                            paintScheduled = true
                            Qt.callLater(function() {
                                maxDoaLineCanvas.requestPaint()
                                paintScheduled = false
                            })
                        }
                    }

                    onPaint: {
                        if (!map) return
                        const ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)

                        for (var i = 0; i < doaPinsModel.count; ++i) {
                            var it = doaPinsModel.get(i)
                            if (!mapviewer.isDoaActive(it)) continue

                            var deg = Number(it.doaDeg)
                            if (!isFinite(deg)) continue
                            deg = mapviewer.wrap360(deg)

                            var coord = QtPositioning.coordinate(it.lat, it.lon)
                            var p = map.fromCoordinate(coord, false)
                            const cx = p.x, cy = p.y

                            const lineR = metersToPixelsAt(coord, lineLengthMeters)

                            ctx.save()
                            ctx.translate(cx, cy)
                            ctx.rotate(-map.bearing * Math.PI / 180.0)

                            const rad = (deg - 90) * Math.PI / 180.0
                            const px  = lineR * Math.cos(rad)
                            const py  = lineR * Math.sin(rad)

                            ctx.beginPath()
                            ctx.moveTo(0, 0)
                            ctx.lineTo(px, py)
                            ctx.strokeStyle = "#1DCD9F"
                            ctx.lineWidth = 3
                            ctx.stroke()

                            ctx.restore()
                        }
                    }

                    Connections {
                        target: map
                        function onZoomLevelChanged() { maxDoaLineCanvas.safeRequestPaint() }
                        function onCenterChanged()    { maxDoaLineCanvas.safeRequestPaint() }
                        function onBearingChanged()   { maxDoaLineCanvas.safeRequestPaint() }
                    }
                    Connections {
                        target: doaPinsModel
                        function onCountChanged() { maxDoaLineCanvas.safeRequestPaint() }
                    }
                    Connections {
                        target: mapviewer
                        function onDoaUpdateTickChanged() { maxDoaLineCanvas.safeRequestPaint() }
                        function onCompassTickChanged() { maxDoaLineCanvas.safeRequestPaint() }
                    }

                    Component.onCompleted: safeRequestPaint()
                }

                // ====== (TX marker + sliders + buttons เหมือนเดิม) ======
                MapItemView {
                    model: txHistoryModel
                    delegate: MapQuickItem {
                        z: 1500
                        coordinate: QtPositioning.coordinate(model.lat, model.lon)
                        anchorPoint.x: 3
                        anchorPoint.y: 3
                        sourceItem: Rectangle {
                            width: 6; height: 6; radius: 3
                            color: "#FFB300"
                            opacity: Math.max(0.05, 0.45 - (index * 0.02))
                        }
                    }
                }

                MapQuickItem {
                    id: txMarker
                    z: 2000
                    visible: mapviewer.txEstimate.valid && mapviewer.txVisible
                    coordinate: QtPositioning.coordinate(mapviewer.txEstimate.lat, mapviewer.txEstimate.lon)
                    anchorPoint.x: txRoot.width/2
                    anchorPoint.y: txRoot.height/2

                    sourceItem: Item {
                        id: txRoot
                        width: 240; height: 240

                        property real accPx: Math.max(18, maxDoaLineCanvas.metersToPixelsAt(
                                                           QtPositioning.coordinate(mapviewer.txEstimate.lat, mapviewer.txEstimate.lon),
                                                           Math.max(20, mapviewer.txEstimate.rms * 2.0)
                                                       ))

                        Canvas {
                            id: txCanvas
                            anchors.fill: parent
                            antialiasing: true

                            property real pulse: 0.0
                            NumberAnimation on pulse {
                                from: 0.0; to: 1.0
                                duration: 900
                                loops: Animation.Infinite
                                running: txMarker.visible
                            }

                            onPaint: {
                                const ctx = getContext("2d")
                                ctx.clearRect(0,0,width,height)

                                const cx = width/2, cy = height/2

                                const r0 = txRoot.accPx
                                ctx.beginPath()
                                ctx.arc(cx, cy, r0, 0, 2*Math.PI)
                                ctx.strokeStyle = "rgba(255,179,0,0.35)"
                                ctx.lineWidth = 2
                                ctx.stroke()

                                const rp = r0 * (1.0 + 0.35*txCanvas.pulse)
                                ctx.beginPath()
                                ctx.arc(cx, cy, rp, 0, 2*Math.PI)
                                ctx.strokeStyle = "rgba(255,179,0," + (0.35*(1.0-txCanvas.pulse)) + ")"
                                ctx.lineWidth = 3
                                ctx.stroke()

                                ctx.lineWidth = 3
                                ctx.strokeStyle = "#FFB300"
                                ctx.beginPath()
                                ctx.moveTo(cx-10, cy); ctx.lineTo(cx+10, cy)
                                ctx.moveTo(cx, cy-10); ctx.lineTo(cx, cy+10)
                                ctx.stroke()

                                ctx.beginPath()
                                ctx.arc(cx, cy, 4, 0, 2*Math.PI)
                                ctx.fillStyle = "#FFB300"
                                ctx.fill()

                                ctx.lineWidth = 2
                                ctx.strokeStyle = "rgba(0,0,0,0.65)"
                                ctx.beginPath()
                                ctx.arc(cx, cy, 7, 0, 2*Math.PI)
                                ctx.stroke()
                            }

                            Timer {
                                interval: 60
                                running: txMarker.visible
                                repeat: true
                                onTriggered: txCanvas.requestPaint()
                            }
                            Component.onCompleted: requestPaint()
                        }
                    }
                }

                BearingSlider {
                    id: bearingSlider
                    mapRef: map
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.leftMargin: 10
                    anchors.topMargin: 400
                }

                ZoomSlider {
                    zoomTarget: map
                    zoomMin: 1.022
                    zoomMax: 18.0
                    zoomStep: 0.1
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 20
                    anchors.bottomMargin: 20
                }

                // Reset North
                Item {
                    width: 48; height: 48
                    anchors.bottom: parent.bottom
                    anchors.rightMargin: 30
                    anchors.bottomMargin: 170
                    anchors.right: parent.right

                    Rectangle {
                        radius: width / 2
                        anchors.fill: parent
                        color: "#C7C8CC"; opacity: 0.6
                        MouseArea { anchors.fill: parent; onClicked: map.bearing = 0 }
                    }

                    Image {
                        anchors.centerIn: parent
                        width: 32; height: 32
                        source: "qrc:/iScreenDFqml/images/cardinal-point.png"
                        transformOrigin: Item.Center
                        property real smoothRotation: -map.bearing
                        RotationAnimator on rotation { duration: 300; easing.type: Easing.InOutQuad }
                        onSmoothRotationChanged: rotation = smoothRotation
                    }
                }

                // Follow
                Item {
                    width: 48
                    height: 48
                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.bottomMargin: 110
                    anchors.rightMargin: 30

                    Rectangle {
                        radius: width / 2
                        anchors.fill: parent
                        color: mapviewer.followPositionEnabled ? "#3498db" : "#C7C8CC"
                        opacity: 0.6
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                mapviewer.followPositionEnabled = !mapviewer.followPositionEnabled
                                if (mapviewer.followPositionEnabled) map.center = mapviewer.selectedCoord
                            }
                        }
                    }

                    Image {
                        anchors.centerIn: parent
                        width: 28
                        height: 28
                        source: mapviewer.followPositionEnabled
                            ? "qrc:/iScreenDFqml/images/pin-map.png"
                            : "qrc:/iScreenDFqml/images/pin_disable.png"
                    }
                }
            }
        }
    }

    // ================== THEME BUTTON ==================
    Item {
        id: themeButton
        width: 48
        height: 48
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.bottomMargin: 230
        anchors.rightMargin: 30
        z: 100000

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "#C7C8CC"
            opacity: 0.6

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (mapLoader.item && mapLoader.item.map) {
                        oldLat = mapLoader.item.map.center.latitude
                        oldLon = mapLoader.item.map.center.longitude
                        oldZoom = mapLoader.item.map.zoomLevel
                        oldBearing = mapLoader.item.map.bearing
                    }

                    styleIndex = (styleIndex + 1) % 3

                    Qt.callLater(() => {
                        mapLoader.sourceComponent = null
                        Qt.callLater(() => {
                            mapLoader.sourceComponent = mapComponent
                            Qt.callLater(() => {
                                if (mapLoader.item && mapLoader.item.map) {
                                    mapLoader.item.map.center = QtPositioning.coordinate(oldLat, oldLon)
                                    mapLoader.item.map.zoomLevel = oldZoom
                                    mapLoader.item.map.bearing = oldBearing
                                }
                            })
                        })
                    })
                }
            }
        }

        Image {
            anchors.centerIn: parent
            width: 28
            height: 28
            source: (styleIndex === 0)
                ? "qrc:/iScreenDFqml/images/moon-and-stars.png"
                : (styleIndex === 1)
                    ? "qrc:/iScreenDFqml/images/sun_theme.png"
                    : "qrc:/iScreenDFqml/images/satellite-view.png"
        }
    }

    Rectangle {
        x: 0
        y: 1048
        width: 1920
        height: 32
        color: "#000000"
    }
}
