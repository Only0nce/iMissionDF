// MapViewer.qml  (FULL FILE)
// ✅ LOCK: ALWAYS use +heading (world = heading + doa + offset)
// ✅ FULL(thetaArray) and MAX-ONLY use SAME fixed rule
// ✅ When heading updates -> recompute stored doaDeg/thetaJson immediately
// ✅ Clear caches when device removed/timeout
// ✅ FIX: maxHud bind by selectedKey (no shared hudMax* cache => no DoA mix-up)
// ✅ Pin color: "random-looking" on first sight, but STABLE per key (no RAM growth)
//
// ✅ FIX(ไม่แสดง): MapTiler style เป็น ONLINE -> ต้องปิด offline_mode
// ✅ FIX(ไม่แสดง): ต้องตั้ง map.activeMapType ให้ใช้ currentStyleUrl จริง (additional_style_urls ไม่ได้ apply อัตโนมัติ)
//
// ✅ NEW: maxDoaDelayMs + txStableHoldMs รับจาก C++ "สัญญาณเดียวกัน"
// ✅ NEW: txSnapDistanceM + intersectionMaxResidualM รับจาก C++ "สัญญาณเดียวกัน"
// ✅ NEW: save/restore 4 ค่านี้ผ่าน Settings

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtLocation 5.15
import QtPositioning 5.15
import QtQuick.Window 2.15
import QtGraphicalEffects 1.15
import Qt.labs.settings 1.1
import "./"

Item {
id: mapviewer
width: 1920
height: 1080

signal requestScreenshot()

// ================== STYLE CONFIG (3 styles) ==================
property int styleIndex: 2
property bool restoring: true
property bool restoringMapState: false

property bool useOfflineStyle: false

property string darkStyleUrl:  "https://api.maptiler.com/maps/streets-v2-dark/style.json?key=tLJWazUH6NsLBuQqI5d4"
property string lightStyleUrl: "https://api.maptiler.com/maps/base-v4/style.json?key=tLJWazUH6NsLBuQqI5d4"
property string thirdStyleUrl: "https://api.maptiler.com/maps/hybrid-v4/style.json?key=tLJWazUH6NsLBuQqI5d4"

// ✅ OFFLINE urls (เปิดใช้งานจริง)
property string darkStyleUrl_offline:  "http://127.0.0.1:8080/styles/googlemapbright/style.json"
property string lightStyleUrl_offline: "http://127.0.0.1:8080/styles/osm-bright-ifz/style.json"
property string thirdStyleUrl_offline: "http://127.0.0.1:8080/styles/satellite-hybrid/style.json"

// ✅ currentStyleUrl เลือกตาม offline/online + styleIndex
property string currentStyleUrl: useOfflineStyle
? ((styleIndex === 0) ? darkStyleUrl_offline
  : (styleIndex === 1) ? lightStyleUrl_offline
  : thirdStyleUrl_offline)
: ((styleIndex === 0) ? darkStyleUrl
  : (styleIndex === 1) ? lightStyleUrl
  : thirdStyleUrl)


property bool isDarkTheme: (styleIndex === 0)

property real oldLat: 0
property real oldLon: 0
property real oldZoom: 13
property real oldBearing: 0

property bool compassVisible: false
property bool navigationEnabled: false
property bool followPositionEnabled: false

Settings {
id: uiSettings
category: "MapViewer"

// ===== UI state =====
property int  savedStyleIndex: 0
property bool savedFollow: false
property real savelineLengthMeters: 15000

property bool savedUseOfflineStyle: false

// ✅ NEW: DOA/TX params from C++
property int  savedMaxDoaDelayMs: 2000
property int  savedTxStableHoldMs: 2000
property real savedTxSnapDistanceM: 250
property real savedIntersectionMaxResidualM: 250

// ===== Map state =====
property real savedLat: 13.75
property real savedLon: 100.5
property real savedZoom: 13
property real savedBearing: 0
}

// ================== MAP RELOAD (preserve state) ==================
function reloadMapAndRestore() {
// snapshot current map state
if (mapLoader.item && mapLoader.item.map) {
    oldLat     = mapLoader.item.map.center.latitude
    oldLon     = mapLoader.item.map.center.longitude
    oldZoom    = mapLoader.item.map.zoomLevel
    oldBearing = mapLoader.item.map.bearing

    uiSettings.savedLat     = oldLat
    uiSettings.savedLon     = oldLon
    uiSettings.savedZoom    = oldZoom
    uiSettings.savedBearing = oldBearing
}

Qt.callLater(function() {
    mapLoader.sourceComponent = null
    Qt.callLater(function() {
        mapLoader.sourceComponent = mapComponent
        Qt.callLater(function() {
            if (mapLoader.item && mapLoader.item.map) {
                var m = mapLoader.item.map
                restoringMapState = true
                m.center    = QtPositioning.coordinate(oldLat, oldLon)
                m.zoomLevel = oldZoom
                m.bearing   = oldBearing

                // apply style again after reload
                Qt.callLater(function() {
                    if (m.applyCurrentStyle) m.applyCurrentStyle()
                    restoringMapState = false
                })
            }
        })
    })
})
}

Component.onCompleted: {
restoring = true

// 1) restore theme index
styleIndex = uiSettings.savedStyleIndex

// 2) restore UI state
followPositionEnabled = uiSettings.savedFollow

useOfflineStyle = uiSettings.savedUseOfflineStyle

// ✅ NEW: restore DOA/TX params
maxDoaDelayMs = uiSettings.savedMaxDoaDelayMs
txStableHoldMs = uiSettings.savedTxStableHoldMs
txSnapDistanceM = uiSettings.savedTxSnapDistanceM
intersectionMaxResidualM = uiSettings.savedIntersectionMaxResidualM

// 3) restore map state
oldLat     = uiSettings.savedLat
oldLon     = uiSettings.savedLon
oldZoom    = uiSettings.savedZoom
oldBearing = uiSettings.savedBearing

// 4) โหลด map หลัง restore เสร็จ (กันโหลดด้วยค่า default)
Qt.callLater(function() {
    mapLoader.sourceComponent = mapComponent

    Qt.callLater(function() {
        if (mapLoader.item && mapLoader.item.map) {
            var m = mapLoader.item.map
            restoringMapState = true
            m.center    = QtPositioning.coordinate(oldLat, oldLon)
            m.zoomLevel = oldZoom
            m.bearing   = oldBearing

            Qt.callLater(function(){
                if (m.applyCurrentStyle) m.applyCurrentStyle()
                restoringMapState = false
            })
        }
        restoring = false
    })
})
}

// เซฟค่าทุกครั้งที่เปลี่ยน
onStyleIndexChanged: {
if (restoring) return
uiSettings.savedStyleIndex = styleIndex
reloadMapAndRestore()
}
onFollowPositionEnabledChanged: uiSettings.savedFollow = mapviewer.followPositionEnabled

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
// DOA REFERENCE (LOCKED to +heading)
// =====================================================================
property bool doaUseHeadingReference: true
property bool doaFlipSign: false
property real doaOffsetDeg: 0

// ================== MAX DOA DELAY SEND STATE ==================
property int  maxDoaDelayMs: 2000          // ✅ หน่วง 2 วิ ก่อนส่ง (รับจาก C++ ได้)
property real maxDoaChangeEpsDeg: 1.0      // ✅ ถือว่า "เปลี่ยน" ถ้า doa ต่างเกิน eps

property string _maxDoaPendingKey: ""
property real   _maxDoaPendingDoa: NaN
property real   _maxDoaPendingConf: NaN
property real   _maxDoaPendingHeading: NaN
property real   _maxDoaPendingLat: NaN
property real   _maxDoaPendingLon: NaN
property double _maxDoaPendingSinceMs: 0

property string _maxDoaLastSentKey: ""
property real   _maxDoaLastSentDoa: NaN
property double _maxDoaLastSentAtMs: 0

function _abs(x){ return Math.abs(Number(x)) }
function _sameDeg(a,b,eps){
a = Number(a); b = Number(b); eps = Number(eps)
if (!isFinite(a) || !isFinite(b)) return false
if (!isFinite(eps) || eps <= 0) eps = 0.5
// wrap-safe difference
var da = mapviewer.wrap360(a)
var db = mapviewer.wrap360(b)
var d  = Math.abs(da - db)
d = Math.min(d, 360 - d)
return d <= eps
}
function wrap360(deg) {
deg = Number(deg)
if (!isFinite(deg)) return 0
deg = deg % 360
if (deg < 0) deg += 360
return deg
}

// ✅ NEW: clean number helper for C++ values (string/number)
function _toNumberClean(v, fallback) {
    var x = v
    if (typeof x === "string") {
        x = x.replace(/,/g, "").replace(/[^\d.\-]/g, "")
    }
    x = Number(x)
    if (!isFinite(x)) x = Number(fallback)
    return isFinite(x) ? x : 0
}

// ============================================================
// MGRS (WGS84) helper: lat/lon -> MGRS
// - precision: digits per easting/northing (1..5) => 10km..1m
// ============================================================
property int txMgrsPrecision: 5  // 5=1m, 4=10m, 3=100m, 2=1km, 1=10km

function _mgrsLatBandLetter(lat) {
    // bands C..X (no I, O); X is 12°
    const bands = "CDEFGHJKLMNPQRSTUVWX"
    var lt = Number(lat)
    if (!isFinite(lt)) lt = 0
    if (lt > 84) lt = 84
    if (lt < -80) lt = -80
    var idx = Math.floor((lt + 80) / 8)
    if (idx < 0) idx = 0
    if (idx > bands.length - 1) idx = bands.length - 1
    return bands.charAt(idx)
}

function _utmZone(lon) {
    var lo = Number(lon)
    if (!isFinite(lo)) lo = 0
    var z = Math.floor((lo + 180) / 6) + 1
    if (z < 1) z = 1
    if (z > 60) z = 60
    return z
}

function _latLonToUTM(lat, lon) {
    // WGS84 -> UTM (zone, easting, northing, hemisphere)
    const a = 6378137.0
    const f = 1.0 / 298.257223563
    const k0 = 0.9996
    const e2 = f * (2 - f)
    const ep2 = e2 / (1 - e2)

    var lt = Number(lat), lo = Number(lon)
    if (!isFinite(lt)) lt = 0
    if (!isFinite(lo)) lo = 0

    var zone = _utmZone(lo)
    var hemi = (lt >= 0) ? "N" : "S"

    var lon0 = (zone - 1) * 6 - 180 + 3 // central meridian
    var phi = lt * Math.PI / 180.0
    var lam = lo * Math.PI / 180.0
    var lam0 = lon0 * Math.PI / 180.0

    var sinPhi = Math.sin(phi)
    var cosPhi = Math.cos(phi)
    var tanPhi = Math.tan(phi)

    var N = a / Math.sqrt(1 - e2 * sinPhi * sinPhi)
    var T = tanPhi * tanPhi
    var C = ep2 * cosPhi * cosPhi
    var A = (lam - lam0) * cosPhi

    // meridional arc
    var M = a * ((1 - e2/4 - 3*e2*e2/64 - 5*e2*e2*e2/256) * phi
             - (3*e2/8 + 3*e2*e2/32 + 45*e2*e2*e2/1024) * Math.sin(2*phi)
             + (15*e2*e2/256 + 45*e2*e2*e2/1024) * Math.sin(4*phi)
             - (35*e2*e2*e2/3072) * Math.sin(6*phi))

    var easting = k0 * N * (A + (1 - T + C) * Math.pow(A,3)/6
                 + (5 - 18*T + T*T + 72*C - 58*ep2) * Math.pow(A,5)/120) + 500000.0

    var northing = k0 * (M + N * tanPhi * (A*A/2
                 + (5 - T + 9*C + 4*C*C) * Math.pow(A,4)/24
                 + (61 - 58*T + T*T + 600*C - 330*ep2) * Math.pow(A,6)/720))

    if (hemi === "S") northing += 10000000.0

    return { zone: zone, hemi: hemi, e: easting, n: northing }
}

function _mgrs100kLetters(zone, easting, northing) {
    // 100k grid letters sets
    const colSets = ["ABCDEFGH", "JKLMNPQR", "STUVWXYZ"]
    const rowSet  = "ABCDEFGHJKLMNPQRSTUV" // 20 letters (no I,O)

    var z = Number(zone)
    var e = Number(easting)
    var n = Number(northing)
    if (!isFinite(z)) z = 1
    if (!isFinite(e)) e = 0
    if (!isFinite(n)) n = 0

    // column letter depends on zone mod 3
    var colSet = colSets[(z - 1) % 3]
    var colIndex = Math.floor(e / 100000) - 1 // 100k columns start at 1
    colIndex = ((colIndex % 8) + 8) % 8
    var colLetter = colSet.charAt(colIndex)

    // row letter depends on zone mod 2 (offset)
    // rows repeat every 2,000,000m => 20 x 100k
    var rowIndex = Math.floor((n % 2000000) / 100000)
    rowIndex = ((rowIndex % 20) + 20) % 20

    var rowOffset = ((z % 2) === 0) ? 5 : 0
    var rowLetter = rowSet.charAt((rowIndex + rowOffset) % 20)

    return colLetter + rowLetter
}

function latLonToMGRS(lat, lon, precision) {
    var lt = Number(lat), lo = Number(lon)
    if (!isFinite(lt) || !isFinite(lo)) return "-"

    // clamp for UTM coverage (standard MGRS/UTM bounds)
    if (lt > 84 || lt < -80) return "-"

    var p = Number(precision)
    if (!isFinite(p)) p = 5
    p = Math.max(1, Math.min(5, Math.round(p)))

    var utm = _latLonToUTM(lt, lo)
    var zone = utm.zone
    var band = _mgrsLatBandLetter(lt)
    var grid = _mgrs100kLetters(zone, utm.e, utm.n)

    var e100k = Math.floor(utm.e % 100000)
    var n100k = Math.floor(utm.n % 100000)

    // precision digits: scale down to required digits
    var div = Math.pow(10, 5 - p)
    var eP = Math.floor(e100k / div)
    var nP = Math.floor(n100k / div)

    function pad(num, len) {
        var s = String(Math.floor(Math.abs(num)))
        while (s.length < len) s = "0" + s
        return s
    }

    return String(zone) + band + " " + grid + " " + pad(eP, p) + " " + pad(nP, p)
}
// ==========================MGRS END ==============================

// ================== COLOR BY KEY (match GPS pin) ==================
function colorRgbaByKey(key, a) {
a = Number(a)
if (!isFinite(a)) a = 1.0
a = Math.max(0, Math.min(1, a))

var gi = findGpsIndexByKey(key)
if (gi >= 0) {
    var it = gpsPinsModel.get(gi)
    var r = Number(it.r), g = Number(it.g), b = Number(it.b)
    if (isFinite(r) && isFinite(g) && isFinite(b)) {
        r = Math.max(0, Math.min(255, Math.round(r)))
        g = Math.max(0, Math.min(255, Math.round(g)))
        b = Math.max(0, Math.min(255, Math.round(b)))
        return "rgba(" + r + "," + g + "," + b + "," + a + ")"
    }
}
// fallback
return "rgba(29,205,159," + a + ")"
}

// ✅ LOCKED: ALWAYS +heading
function worldDoaPlus(rawDoaDeg, headingDeg, offsetDeg) {
var d = Number(rawDoaDeg)
var h = Number(headingDeg)
var off = Number(offsetDeg || 0)
if (!isFinite(d)) d = 0
if (!isFinite(h)) h = 0
if (!isFinite(off)) off = 0

if (mapviewer.doaFlipSign) d = -d
// world = heading + doa + offset
return wrap360(h + d + off)
}

function rotateThetaArrayToWorldPlus(thetaArray, headingDeg, offsetDeg) {
if (!thetaArray) return []
var h = Number(headingDeg)
var off = Number(offsetDeg || 0)
if (!isFinite(h)) h = 0
if (!isFinite(off)) off = 0

var out = new Array(thetaArray.length)
for (var i = 0; i < thetaArray.length; ++i) {
    var th = Number(thetaArray[i])
    if (!isFinite(th)) th = 0
    if (mapviewer.doaFlipSign) th = -th
    out[i] = wrap360(h + th + off)
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

function headingOfKey(key) {
var gi = findGpsIndexByKey(key)
if (gi >= 0) return Number(gpsPinsModel.get(gi).headingDeg || 0)
return 0
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
Qt.callLater(function() {
    mapviewer.doaUpdateTick++
})
}

// =====================================================================
// ✅ RECOMPUTE stored DOA when heading changes (LOCKED +heading)
// =====================================================================
function recomputeDoaForKey(key, newHeadingDeg) {
var di = findDoaIndexByKey(key)
if (di < 0) return

var it = doaPinsModel.get(di)
if (!it) return

var c = coordOfKey(key)
if (!c) return

var h = Number(newHeadingDeg)
if (!isFinite(h)) h = 0

var raw = Number(it.doaRawDeg)
if (!isFinite(raw)) raw = 0

var worldDeg = worldDoaPlus(raw, h, mapviewer.doaOffsetDeg)

var rawThetaArr = []
try { rawThetaArr = JSON.parse(it.thetaRawJson || "[]") } catch(e) { rawThetaArr = [] }
var thetaWorldArr = rotateThetaArrayToWorldPlus(rawThetaArr, h, mapviewer.doaOffsetDeg)

doaPinsModel.set(di, {
    key: key,
    lat: c.latitude,
    lon: c.longitude,
    updatedMs: it.updatedMs,
    firstSeenMs: it.firstSeenMs,

    thetaRawJson: it.thetaRawJson,
    thetaJson: JSON.stringify(thetaWorldArr),
    rJson: it.rJson,

    doaDeg: worldDeg,
    doaRawDeg: raw,
    confidence: it.confidence,
    headingDeg: h
})
}

function upsertCompass(serial, name, headingDeg) {
var k = keyOf(serial, name)
var h = Number(headingDeg)
if (!isFinite(h)) return

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
recomputeDoaForKey(k, h)

compassTick++
if (mapLoader.item && mapLoader.item.map) {
    if (mapLoader.item.headingLineCanvas) mapLoader.item.headingLineCanvas.safeRequestPaint()
    if (mapLoader.item.maxDoaLineCanvas)  mapLoader.item.maxDoaLineCanvas.safeRequestPaint()
}
_kickRepaintAllDoa()
}

function upsertDoaFrame(serial, controllerName, thetaArray, spectrumArray, doaDeg, confidence, headingDeg) {
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
        headingDeg: Number(headingDeg),
        updatedMs: Date.now()
    }
    return
}

var h = Number(headingDeg)
if (!isFinite(h)) h = 0

var doaWorldDeg   = worldDoaPlus(doaDeg, h, mapviewer.doaOffsetDeg)
var thetaWorldArr = rotateThetaArrayToWorldPlus(thetaArray || [], h, mapviewer.doaOffsetDeg)

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
    headingDeg: h
}

if (idx >= 0) doaPinsModel.set(idx, obj)
else doaPinsModel.append(obj)
}

function upsertMaxOnlyDoa(serial, controllerName, doaDeg, confidence, headingDeg) {
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
        headingDeg: Number(headingDeg),
        updatedMs: Date.now()
    }
    return
}

var h = Number(headingDeg)
if (!isFinite(h)) h = 0

var doaWorldDeg = worldDoaPlus(doaDeg, h, mapviewer.doaOffsetDeg)

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
    headingDeg: h
}

if (idx >= 0) doaPinsModel.set(idx, obj)
else doaPinsModel.append(obj)
}

// ================== STABLE COLOR BY KEY (no cache growth) ==================
// ทำให้ "เหมือนสุ่มครั้งแรก" แต่ key เดิมจะได้สีเดิมเสมอ โดยไม่ต้องจำใน RAM

function _clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)) }

// FNV-1a 32-bit hash
function _fnv1a32(str) {
str = (str !== undefined && str !== null) ? String(str) : ""
var h = 0x811c9dc5
for (var i = 0; i < str.length; ++i) {
    h ^= str.charCodeAt(i)
    h = (h + ((h << 1) + (h << 4) + (h << 7) + (h << 8) + (h << 24))) >>> 0
}
return h >>> 0
}

function _hslToRgb(h, s, l) {
h = (Number(h) % 360 + 360) % 360
s = _clamp(Number(s), 0, 1)
l = _clamp(Number(l), 0, 1)

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
r = _clamp(r, 0, 255); g = _clamp(g, 0, 255); b = _clamp(b, 0, 255)
return { r:r, g:g, b:b }
}

// ✅ key เดิม => สีเดิมเสมอ
function pickStableColorByKey(key) {
var h = _fnv1a32(key)
var hue = (h % 360)
return _hslToRgb(hue, 0.90, 0.55)
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
    // ✅ สี stable ต่อ key -> ไม่ต้องจำใน map/cache -> RAM ไม่โต
    var rgb = pickStableColorByKey(key)

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
// ✅ HUD helper: get active DOA item by selected key (fix DoA mix-up)
// =====================================================================
function getActiveDoaItemByKey(key) {
if (!key || !key.length) return null
var idx = findDoaIndexByKey(key)
if (idx < 0) return null
var it = doaPinsModel.get(idx)
if (!it) return null
if (!isDoaActive(it)) return null
return it
}

// ===== TX HISTORY COMMIT WHEN MARK SHOWS =====
property bool _txMarkWasVisible: false

function commitTxHistoryNow() {
if (!txVisible) return
if (!txEstimate || !txEstimate.valid) return

if (txHistoryModel.count === 0) {
    txHistoryModel.insert(0, {
        lat: txEstimate.lat,
        lon: txEstimate.lon,
        rms: txEstimate.rms,
        updatedMs: txEstimate.updatedMs
    })
} else {
    const h = txHistoryModel.get(0)
    const d = _haversineMeters(txEstimate.lat, txEstimate.lon, h.lat, h.lon)

    if (d <= txSnapDistanceM) {
        txHistoryModel.set(0, {
            lat: h.lat,
            lon: h.lon,
            rms: txEstimate.rms,
            updatedMs: txEstimate.updatedMs
        })
    } else {
        txHistoryModel.insert(0, {
            lat: txEstimate.lat,
            lon: txEstimate.lon,
            rms: txEstimate.rms,
            updatedMs: txEstimate.updatedMs
        })
    }
}

while (txHistoryModel.count > txHistoryMax)
    txHistoryModel.remove(txHistoryModel.count - 1)
}

// =====================================================================
// TX ESTIMATE
// =====================================================================
property bool txVisible: true
property var  txEstimate: ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0 })
property var  txCandidate: ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0, firstSeenMs:0 })

property int  txStableHoldMs: 2000   // ✅ รับจาก C++

property int  txHistoryMax: 100
ListModel { id: txHistoryModel }

property real intersectionMaxResidualM: 250   // ✅ รับจาก C++
property real txSnapDistanceM: 250            // ✅ รับจาก C++

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

function currentMaxDoaLineMeters() {
if (mapLoader.item && mapLoader.item.maxDoaLineCanvas) {
    var m = Number(mapLoader.item.maxDoaLineCanvas.lineLengthMeters)
    if (isFinite(m) && m > 0) return m
}
var s = Number(uiSettings.savelineLengthMeters)
if (isFinite(s) && s > 0) return s
return 15000
}

function rebuildTxEstimateKrakenLike() {
if (!txVisible) {
    txEstimate   = ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0 })
    txCandidate  = ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0, firstSeenMs:0 })
    txHistoryModel.clear()
    return
}

let items = []
for (let i=0; i<doaPinsModel.count; ++i) {
    const it = doaPinsModel.get(i)
    if (!mapviewer.isDoaActive(it)) continue

    // ✅ NEW: hide from TX calc if switch OFF
    if (hudBox && hudBox.isKeyHidden(it.key)) continue

    const lat = Number(it.lat), lon = Number(it.lon)
    let deg = Number(it.doaDeg)
    if (!isFinite(lat) || !isFinite(lon) || !isFinite(deg)) continue
    deg = mapviewer.wrap360(deg)

    items.push({ lat:lat, lon:lon, deg:deg, key:it.key })
}

if (items.length < 2) {
    txEstimate   = ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0 })
    txCandidate  = ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0, firstSeenMs:0 })
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

const maxLineM = Number(currentMaxDoaLineMeters())
const maxM = (isFinite(maxLineM) && maxLineM > 0) ? maxLineM : 15000

let x=0, y=0, rms=0

if (items.length === 2) {
    const r = _intersect2Lines(pts[0], uvs[0], pts[1], uvs[1])
    if (!r.ok) {
        txEstimate  = ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0 })
        txCandidate = ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0, firstSeenMs:0 })
        return
    }

    if (!(r.t >= 0 && r.t <= maxM && r.s >= 0 && r.s <= maxM)) {
        txEstimate  = ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0 })
        txCandidate = ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0, firstSeenMs:0 })
        return
    }

    x = r.x; y = r.y; rms = 0
} else {
    const best = _bestPointForLines(pts, uvs)
    if (!best.ok || best.rms > mapviewer.intersectionMaxResidualM) {
        txEstimate  = ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0 })
        txCandidate = ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0, firstSeenMs:0 })
        return
    }

    for (let i=0; i<pts.length; ++i) {
        const dx = best.x - pts[i].x
        const dy = best.y - pts[i].y
        const t  = dx*uvs[i].ux + dy*uvs[i].uy
        if (!(t >= 0 && t <= maxM)) {
            txEstimate  = ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0 })
            txCandidate = ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0, firstSeenMs:0 })
            return
        }
    }

    x = best.x; y = best.y; rms = best.rms
}

const ll = _xyToLatLon(x, y, refLat, refLon)

let cand = ({
    valid: true,
    lat: ll.lat,
    lon: ll.lon,
    rms: rms,
    count: items.length,
    updatedMs: Date.now(),
    firstSeenMs: 0
})

if (txCandidate.valid) {
    const dSame = _haversineMeters(cand.lat, cand.lon, txCandidate.lat, txCandidate.lon)
    if (dSame <= mapviewer.txSnapDistanceM) {
        cand.lat = txCandidate.lat
        cand.lon = txCandidate.lon
        cand.firstSeenMs = Number(txCandidate.firstSeenMs || txCandidate.updatedMs || Date.now())
    } else {
        cand.firstSeenMs = Date.now()
    }
} else {
    cand.firstSeenMs = Date.now()
}

txCandidate = cand

const ageMs = Date.now() - Number(txCandidate.firstSeenMs || 0)
const stableOk = isFinite(ageMs) && ageMs >= txStableHoldMs

if (!stableOk) {
    txEstimate = ({ valid:false, lat:0, lon:0, rms:0, count:0, updatedMs:0 })
    return
}

txEstimate = ({
    valid: true,
    lat: txCandidate.lat,
    lon: txCandidate.lon,
    rms: txCandidate.rms,
    count: txCandidate.count,
    updatedMs: Date.now()
})

if (txHistoryModel.count === 0) {
    txHistoryModel.insert(0, {
        lat: txEstimate.lat,
        lon: txEstimate.lon,
        rms: txEstimate.rms,
        updatedMs: txEstimate.updatedMs
    })
} else {
    const h = txHistoryModel.get(0)
    const d2 = _haversineMeters(txEstimate.lat, txEstimate.lon, h.lat, h.lon)

    if (d2 <= mapviewer.txSnapDistanceM) {
        txHistoryModel.set(0, {
            lat: h.lat,
            lon: h.lon,
            rms: txEstimate.rms,
            updatedMs: txEstimate.updatedMs
        })
    } else {
        txHistoryModel.insert(0, {
            lat: txEstimate.lat,
            lon: txEstimate.lon,
            rms: txEstimate.rms,
            updatedMs: txEstimate.updatedMs
        })
    }
}

while (txHistoryModel.count > txHistoryMax)
    txHistoryModel.remove(txHistoryModel.count - 1)
}

// =====================================================================
// MAX DOA monitor (for logger only) - HUD reads from selectedKey directly
// =====================================================================
function updateMaxDoaMonitor() {
var best = null
var bestConf = -1

for (var i = 0; i < doaPinsModel.count; ++i) {
    var it = doaPinsModel.get(i)
    if (!mapviewer.isDoaActive(it)) continue

    var conf = Number(it.confidence || 0)
    if (!isFinite(conf)) conf = 0
    if (conf > bestConf) { bestConf = conf; best = it }
}

if (!best) {
    _maxDoaPendingKey = ""
    _maxDoaPendingSinceMs = 0
    return
}

var keyNow = String(best.key || "")
var doaNow = Number(best.doaDeg || 0)
var confNow = Number(best.confidence || 0)
var headingNow = Number(best.headingDeg || 0)
var latNow = Number(best.lat || 0)
var lonNow = Number(best.lon || 0)

if (!keyNow.length || !isFinite(doaNow) || !isFinite(confNow)) return

var changed = false

if (_maxDoaPendingKey !== keyNow) {
    changed = true
} else {
    if (!_sameDeg(_maxDoaPendingDoa, doaNow, maxDoaChangeEpsDeg)) changed = true
}

if (changed) {
    _maxDoaPendingKey = keyNow
    _maxDoaPendingDoa = doaNow
    _maxDoaPendingConf = confNow
    _maxDoaPendingHeading = headingNow
    _maxDoaPendingLat = latNow
    _maxDoaPendingLon = lonNow
    _maxDoaPendingSinceMs = Date.now()
    return
}

var ageMs = Date.now() - Number(_maxDoaPendingSinceMs || 0)
if (!isFinite(ageMs) || ageMs < maxDoaDelayMs) {
    return
}

if (_maxDoaLastSentKey === _maxDoaPendingKey && _sameDeg(_maxDoaLastSentDoa, _maxDoaPendingDoa, maxDoaChangeEpsDeg)) {
    return
}

if (viewerHud && viewerHud.logger) {
    viewerHud.logger.feedMaxDoaCandidate({
        key: String(_maxDoaPendingKey || ""),
        doa: Number(_maxDoaPendingDoa || 0),
        confidence: Number(_maxDoaPendingConf || 0),
        heading: Number(_maxDoaPendingHeading || 0),
        lat: Number(_maxDoaPendingLat || 0),
        lon: Number(_maxDoaPendingLon || 0)
    })
}

_maxDoaLastSentKey = _maxDoaPendingKey
_maxDoaLastSentDoa = _maxDoaPendingDoa
_maxDoaLastSentAtMs = Date.now()
}

// ================== HUD: COMPASS + MAX DOA ==================
property bool showCompassHud: true
property bool showMaxDoaHud:  true

function _fmtDeg(v) {
var x = Number(v)
if (!isFinite(x)) x = 0
x = wrap360(x)
return x.toFixed(0) + "°"
}
function _fmtConf(v){
var x = Number(v)
if (!isFinite(x)) x = 0
return x.toFixed(2)
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

            // ✅ clear doa + pending caches for this key
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
    }
}
}

// ================== MAP LOADER ==================
Loader {
id: mapLoader
anchors.fill: parent
sourceComponent: null
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
anchors.topMargin: 280
anchors.rightMargin: 30

txModel: txHistoryModel
rfCache: rfcache
krakenmapval: Krakenmapval
}

onUseOfflineStyleChanged: {
if (restoring) return
uiSettings.savedUseOfflineStyle = useOfflineStyle
reloadMapAndRestore()
}

Connections {
target: Krakenmapval
function onUseOfflineMapStyleChanged(mapStatus) {
    var v = !!mapStatus
    if (mapviewer.useOfflineStyle === v) return

    mapviewer.useOfflineStyle = v
    uiSettings.savedUseOfflineStyle = v

    // ✅ รีโหลด map เพื่อให้ plugin parameter + style url ใช้งานจริง
    mapviewer.reloadMapAndRestore()
}
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

// ✅ NEW: รับ 4 พารามิเตอร์จาก C++ (คู่ค่าใช้สัญญาณเดียวกัน)

Connections {
    target: Krakenmapval
    ignoreUnknownSignals: true

    // ✅ 1 signal updates BOTH maxDoaDelayMs + txStableHoldMs
    function onUpdateMaxDoaDelayMsFromServer(ms) {
        var md = mapviewer._toNumberClean(ms, mapviewer.maxDoaDelayMs)
        md = Math.max(0, Math.round(md))

        // maxDoaDelayMs
        if (mapviewer.maxDoaDelayMs !== md) {
            mapviewer.maxDoaDelayMs = md
            uiSettings.savedMaxDoaDelayMs = md
        }

        // txStableHoldMs (use same value)
        if (mapviewer.txStableHoldMs !== md) {
            mapviewer.txStableHoldMs = md
            uiSettings.savedTxStableHoldMs = md
        }
    }

    // ✅ 1 signal updates BOTH txSnapDistanceM + intersectionMaxResidualM
    function onUpdateDoaLineDistanceMFromServer(meters) {
        var sd = mapviewer._toNumberClean(meters, mapviewer.txSnapDistanceM)
        sd = Math.max(0, sd)

        // txSnapDistanceM
        if (mapviewer.txSnapDistanceM !== sd) {
            mapviewer.txSnapDistanceM = sd
            uiSettings.savedTxSnapDistanceM = sd
        }

        // intersectionMaxResidualM (use same value)
        if (mapviewer.intersectionMaxResidualM !== sd) {
            mapviewer.intersectionMaxResidualM = sd
            uiSettings.savedIntersectionMaxResidualM = sd
        }
    }

    // (optional) keep compatibility if C++ still emits old combined signals
    function onUpdateDoaDelayAndTxHold(maxDelayMs, stableHoldMs) {
        // take maxDelayMs as the "source of truth" for both
        onUpdateMaxDoaDelayMsFromServer(maxDelayMs)
    }

    function onUpdateTxSnapAndResidual(snapDistanceM, residualM) {
        // take snapDistanceM as the "source of truth" for both
        onUpdateDoaLineDistanceMFromServer(snapDistanceM)
    }

    function onUpdateDoaTxParams(maxDelayMs, stableHoldMs, snapDistanceM, residualM) {
        onUpdateMaxDoaDelayMsFromServer(maxDelayMs)
        onUpdateDoaLineDistanceMFromServer(snapDistanceM)
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

        // ✅ FIX: online style => ต้อง OFF offline_mode ไม่งั้นจะดำ/ว่าง
        // ✅ online/offline mode ต้องสลับตาม useOfflineStyle
        PluginParameter {
            name: "mapboxgl.mapping.offline_mode"
            value: mapviewer.useOfflineStyle ? "true" : "false"
        }

        // ✅ เปิด cache (ช่วยให้ไม่ blank และโหลดลื่นขึ้น)
        PluginParameter { name: "mapboxgl.mapping.tilecache.enable"; value: "true" }
        PluginParameter { name: "mapboxgl.mapping.cache.database"; value: "cache.db" }

        PluginParameter { name: "mapboxgl.mapping.cache.memory"; value: "32" }
        PluginParameter { name: "mapboxgl.mapping.cache.size"; value: "52428800" }
        PluginParameter { name: "mapboxgl.mapping.transitions.fadeDuration"; value: "0" }

        // ✅ เพิ่ม style url
        PluginParameter { name: "mapboxgl.mapping.additional_style_urls"; value: mapviewer.currentStyleUrl }
    }

    Map {
        id: map
        anchors.fill: parent
        plugin: mapboxPlugin
        center: QtPositioning.coordinate(13.75, 100.5)
        zoomLevel: 13
        maximumZoomLevel: 18

        // ============================================================
        // ✅ FIX: apply currentStyleUrl to activeMapType
        // ============================================================
        function applyCurrentStyle() {
            if (!supportedMapTypes || supportedMapTypes.length === 0) return
            var target = String(mapviewer.currentStyleUrl || "")
            if (!target.length) return

            for (var i = 0; i < supportedMapTypes.length; ++i) {
                var t = supportedMapTypes[i]
                if (!t) continue
                if (String(t.name || "") === target) {
                    activeMapType = t
                    return
                }
            }

            // fallback: ใช้ตัวแรกกันจอว่าง
            activeMapType = supportedMapTypes[0]
        }

        onSupportedMapTypesChanged: {
            Qt.callLater(function(){ map.applyCurrentStyle() })
        }

        Connections {
            target: mapviewer
            function onCurrentStyleUrlChanged() {
                // ถ้าไม่ reload map ก็ยัง apply ได้ (แต่ของคุณ reload อยู่แล้ว)
                Qt.callLater(function(){ map.applyCurrentStyle() })
            }
        }

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
                var k = mapviewer.keyOf(s, n)

                var headingNow = mapviewer.headingOfKey(k)

                var thOk = thetaArray && thetaArray.length >= 2
                var spOk = spectrumArray && spectrumArray.length >= 2

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

        onCenterChanged: {
            if (mapviewer.restoringMapState) return
            if (mapviewer.followPositionEnabled) return
            if (isFinite(center.latitude) && isFinite(center.longitude)) {
                uiSettings.savedLat = center.latitude
                uiSettings.savedLon = center.longitude
            }
        }

        onZoomLevelChanged: {
            if (mapviewer.restoringMapState) return
            if (isFinite(zoomLevel)) uiSettings.savedZoom = zoomLevel
        }

        onBearingChanged: {
            if (mapviewer.restoringMapState) return
            if (isFinite(bearing)) uiSettings.savedBearing = bearing
        }

        Component.onCompleted: {
            mapviewer.restoringMapState = true
            center    = QtPositioning.coordinate(mapviewer.oldLat, mapviewer.oldLon)
            zoomLevel = mapviewer.oldZoom
            bearing   = mapviewer.oldBearing

            Qt.callLater(function() {
                map.applyCurrentStyle()
                mapviewer.restoringMapState = false
            })
        }

        // ================= MULTI PIN GPS =================
        MapItemView {
            id: gpsPinsView
            model: gpsPinsModel

            delegate: MapQuickItem {
                id: onePin
                z: (mapviewer.selectedKey === model.key) ? 50 : 10

                // ✅ NEW: hide pin when switch OFF
                visible: !hudBox.isKeyHidden(model.key)

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
        // DOA OVERLAYS (FULL FRAME only)
        // ============================================================
        MapItemView {
            id: doaOverlaysView
            model: doaPinsModel
            visible: mapviewer.hasAnyActiveDoa()

            delegate: MapQuickItem {
                id: doaOverlayItem
                z: 0

                // ✅ NEW: hide overlay when switch OFF
                visible: mapviewer.isDoaActive(doaPinsModel.get(index))
                         && mapviewer.isFullDoaItem(doaPinsModel.get(index))
                         && !hudBox.isKeyHidden(doaPinsModel.get(index).key)

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

                        // ✅ NEW: safety hide if switched off
                        var it0 = doaPinsModel.get(index)
                        if (it0 && hudBox.isKeyHidden(it0.key)) return

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

        // ============ HEADING LINES (MULTI) ============
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

                    // ✅ NEW: skip if hidden by switch
                    if (hudBox && hudBox.isKeyHidden(it.key)) continue

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

        // ============ MAX DOA LINES (MULTI) ============
        Canvas {
            id: maxDoaLineCanvas
            anchors.fill: parent
            antialiasing: true
            z: 9999

            visible: mapviewer.hasAnyActiveMaxDoa()
            onVisibleChanged: safeRequestPaint()

            property bool paintScheduled: false

            // ✅ THIS is the real line length used for drawing (meters)
            property real lineLengthMeters: 15000

            onLineLengthMetersChanged: {
                console.log("[MAXLINE] lineLengthMeters=", lineLengthMeters)

                var m = Number(lineLengthMeters)
                if (isFinite(m) && m > 0) {
                    uiSettings.savelineLengthMeters = m
                }
                safeRequestPaint()
            }

            function metersToPixelsAt(coord, meters) {
                meters = Number(meters)
                if (!map || !isFinite(meters) || meters <= 0) return 0

                const R = 6378137
                const cosLat = Math.cos(Math.PI * coord.latitude / 180)
                if (!isFinite(cosLat) || cosLat === 0) return 0

                const dLng = (meters / (R * cosLat)) * (180 / Math.PI)
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

                const meters = Number(lineLengthMeters)
                if (!isFinite(meters) || meters <= 0) return

                for (var i = 0; i < doaPinsModel.count; ++i) {
                    var it = doaPinsModel.get(i)
                    if (!mapviewer.isDoaActive(it)) continue

                    // ✅ NEW: skip if hidden by switch
                    if (hudBox && hudBox.isKeyHidden(it.key)) continue

                    var deg = Number(it.doaDeg)
                    if (!isFinite(deg)) continue
                    deg = mapviewer.wrap360(deg)

                    var coord = QtPositioning.coordinate(it.lat, it.lon)
                    var p = map.fromCoordinate(coord, false)
                    const cx = p.x, cy = p.y

                    const lineR = metersToPixelsAt(coord, meters)
                    if (!isFinite(lineR) || lineR <= 0) continue

                    ctx.save()
                    ctx.translate(cx, cy)

                    // align to map north
                    ctx.rotate(-map.bearing * Math.PI / 180.0)

                    const rad = (deg - 90) * Math.PI / 180.0
                    const px  = lineR * Math.cos(rad)
                    const py  = lineR * Math.sin(rad)

                    var colMain = mapviewer.colorRgbaByKey(it.key, 0.95)
                    var colGlow = mapviewer.colorRgbaByKey(it.key, 0.25)

                    // glow
                    ctx.beginPath()
                    ctx.moveTo(0, 0)
                    ctx.lineTo(px, py)
                    ctx.strokeStyle = colGlow
                    ctx.lineWidth = 8
                    ctx.stroke()

                    // main line
                    ctx.beginPath()
                    ctx.moveTo(0, 0)
                    ctx.lineTo(px, py)
                    ctx.strokeStyle = colMain
                    ctx.lineWidth = 3
                    ctx.stroke()

                    ctx.restore()
                }
            }
            // repaint triggers
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
                function onCompassTickChanged()   { maxDoaLineCanvas.safeRequestPaint() }
            }

            // ✅ receive from C++/backend
            Connections {
                target: Krakenmapval
                function onUpdateDoaLineMeters(meters) {
                    var m = meters
                    if (typeof m === "string") {
                        m = m.replace(/,/g, "").replace(/[^\d.]/g, "")
                    }
                    m = Number(m)

                    console.log("[SIG] onUpdateDoaLineMeters meters=", meters, "=>", m)

                    if (isFinite(m) && m > 0) {
                        maxDoaLineCanvas.lineLengthMeters = m
                        uiSettings.savelineLengthMeters = m
                        maxDoaLineCanvas.safeRequestPaint()
                    }
                }
            }

            Component.onCompleted: {
                var m = Number(uiSettings.savelineLengthMeters)
                if (isFinite(m) && m > 0) lineLengthMeters = m
                safeRequestPaint()
            }
        }

        // ============================================================
        // TX MARKER (trail) + main marker
        // ============================================================
        MapItemView {
            model: txHistoryModel
            visible: (txHistoryModel.count > 0) && mapviewer.txVisible

            delegate: MapQuickItem {
                z: 1500
                coordinate: QtPositioning.coordinate(model.lat, model.lon)
                anchorPoint.x: 3
                anchorPoint.y: 3

                sourceItem: Rectangle {
                    width: 6; height: 6; radius: 3
                    color: (mapviewer.styleIndex === 0) ? "#FFB300"
                         : (mapviewer.styleIndex === 1) ? "#2D9CDB"
                         : "#FF4D9D"
                    opacity: Math.max(0.05, 0.45 - (index * 0.02))
                }
            }
        }

        MapQuickItem {
            id: txMarker
            z: 9999
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
                // ===================== MGRS label =====================
                Item {
                    id: mgrsLabel
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 18
                    width: Math.max(160, mgrsText.implicitWidth + 18)
                    height: mgrsText.implicitHeight + 10
                    visible: txMarker.visible


                    Rectangle {
                        anchors.fill: parent
                        radius: 10
                        color: "black"
                        opacity: 0.65

                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.25)   // ✅ FIX
                    }
                    Text {
                        id: mgrsText
                        anchors.centerIn: parent
                        text: "MGRS: " + mapviewer.latLonToMGRS(
                                  mapviewer.txEstimate.lat,
                                  mapviewer.txEstimate.lon,
                                  mapviewer.txMgrsPrecision
                              )
                        color: "#FFFFFF"
                        font.pixelSize: 12
                        font.bold: true
                        style: Text.Outline
                        styleColor: "black"
                    }
                }
            }
        }

        CompassCanvas {
            width: 700
            height: 700
            anchors.centerIn: parent
            visible: mapviewer.compassVisible
            bearing: map.bearing
            innerBearing: map.bearing - (Krakenmapval.degree || 0)
            ringColor: isDarkTheme ? "#00ffff" : "#000000"
            textColor: isDarkTheme ? "#ffffff" : "#000000"
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
                        uiSettings.savedFollow = mapviewer.followPositionEnabled
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
            styleIndex = (styleIndex + 1) % 3
            uiSettings.savedStyleIndex = styleIndex
            mapviewer.reloadMapAndRestore()
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

Item {
   id: hudBox
   visible: mapviewer.showCompassHud || mapviewer.showMaxDoaHud
   z: 0
   anchors.left: parent.left
   anchors.top: parent.top
   anchors.leftMargin: 65
   anchors.topMargin: 120

   property int  hudMaxCards: 12
   property int  cardW: 300
   property int  cardH: 190
   property int  cardGap: 10
   property int  listMaxH: 790

   // ✅ IMPORTANT: do not force selectedKey to be first => list won't jump
   property bool selectedFirst: false

   // ✅ hover/pin highlight states
   property string pinnedKey: ""
   property string hoverKey:  ""

   // ===================== Per-card show/hide pin+doa =====================
   // hiddenKeys[key] === true  -> hide this device on map (pin + doa)
   property var hiddenKeys: ({})

   function isKeyHidden(k) {
       if (!k || !k.length) return false
       return !!hiddenKeys[k]
   }

   function setKeyHidden(k, hide) {
       if (!k || !k.length) return
       var m = hiddenKeys
       if (!m) m = {}
       if (hide) m[k] = true
       else if (m[k] !== undefined) delete m[k]
       hiddenKeys = m // reassign to trigger bindings
   }

   function toggleKeyHidden(k) {
       setKeyHidden(k, !isKeyHidden(k))
   }

   width: cardW
   height: Math.min(listMaxH, (cardH * Math.max(1, hudList.count)) + (cardGap * Math.max(0, hudList.count - 1)))

   ListModel { id: hudList }

   function _isPinnedOrHovered(k) { return (k === pinnedKey) || (k === hoverKey) }

   // ✅ get stable pin color from gpsPinsModel (r,g,b stored in model)
   function _pinColorHexByKey(key) {
       var gi = mapviewer.findGpsIndexByKey(key)
       if (gi >= 0) {
           var it = gpsPinsModel.get(gi)
           var r = Number(it.r), g = Number(it.g), b = Number(it.b)
           if (isFinite(r) && isFinite(g) && isFinite(b)) {
               r = Math.max(0, Math.min(255, Math.round(r)))
               g = Math.max(0, Math.min(255, Math.round(g)))
               b = Math.max(0, Math.min(255, Math.round(b)))
               function hex2(v){ var s=v.toString(16); return (s.length<2) ? ("0"+s) : s }
               return "#" + hex2(r) + hex2(g) + hex2(b)
           }
       }
       return "#00c896"
   }

   function _isKeyValid(key) {
       if (!key || !key.length) return false
       var c = mapviewer.coordOfKey(key)
       return c !== null
   }

   function rebuildHudList() {
       hudList.clear()

       if (selectedFirst && _isKeyValid(mapviewer.selectedKey)) {
           hudList.append({ key: mapviewer.selectedKey })
       }

       var keys = []
       for (var i = 0; i < gpsPinsModel.count; ++i) {
           var it = gpsPinsModel.get(i)
           if (!it || !it.key) continue
           if (!_isKeyValid(it.key)) continue
           if (selectedFirst && it.key === mapviewer.selectedKey) continue
           keys.push({ key: it.key, updatedMs: Number(it.updatedMs || 0) })
       }
       keys.sort(function(a,b){ return (b.updatedMs - a.updatedMs) })

       for (var k = 0; k < keys.length; ++k) {
           if (hudList.count >= hudMaxCards) break
           hudList.append({ key: keys[k].key })
       }

       if (hudList.count === 0 && mapviewer.selectedKey && mapviewer.selectedKey.length) {
           hudList.append({ key: mapviewer.selectedKey })
       }
   }

   Component.onCompleted: rebuildHudList()

   Connections {
       target: gpsPinsModel
       function onCountChanged() { hudBox.rebuildHudList() }
   }
   Connections {
       target: mapviewer
       function onSelectedKeyChanged() { hudBox.rebuildHudList() }
   }

   ListView {
       id: hudListView
       anchors.fill: parent
       model: hudList
       clip: true
       spacing: hudBox.cardGap

       ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

       delegate: Item {
           id: card
           width: hudBox.cardW
           height: hudBox.cardH

           property string cardKey: model.key
           property real headingDeg: mapviewer.headingOfKey(cardKey)
           property var  doaIt: mapviewer.getActiveDoaItemByKey(cardKey)

           // ✅ pin color + highlight rule
           property color pinColor: hudBox._pinColorHexByKey(cardKey)

           // ✅ if hidden => dim
           property bool isHidden: hudBox.isKeyHidden(cardKey)

           property bool  isHi: (!card.isHidden) && ((cardKey === mapviewer.selectedKey) || hudBox._isPinnedOrHovered(cardKey))

           Rectangle {
               anchors.fill: parent
               radius: 16
               color: "#0B1216"
               opacity: card.isHidden ? 0.35 : 0.72
               border.width: 2
               border.color: card.isHidden ? "#2A3A44" : (card.isHi ? card.pinColor : "#2A3A44")
           }

           Row {
               anchors.left: parent.left
               anchors.top: parent.top
               anchors.leftMargin: 14
               anchors.topMargin: 10
               spacing: 8

               Rectangle {
                   width: 8; height: 8; radius: 4
                   color: card.isHidden ? "#69737C" : (card.isHi ? card.pinColor : "#9aa6b2")
                   opacity: 0.9
                   anchors.verticalCenter: parent.verticalCenter
               }

               Text {
                   text: cardKey && cardKey.length ? cardKey : "-"
                   color: "#e6edf3"
                   opacity: card.isHidden ? 0.55 : 1.0
                   font.pixelSize: 11
                   font.bold: true
                   elide: Text.ElideRight
                   width: hudBox.cardW - 14 - 14 - 60 // leave space for switch
               }
           }

           // ✅ ONE SWITCH: show/hide pin+doa
           Switch {
               id: showSwitch
               z: 9999
               anchors.top: parent.top
               anchors.right: parent.right
               anchors.topMargin: 6
               anchors.rightMargin: 10

               // checked = SHOW, unchecked = HIDE
               checked: !hudBox.isKeyHidden(cardKey)

               onToggled: {
                   hudBox.setKeyHidden(cardKey, !checked)

                   // repaint immediately
                   mapviewer.doaUpdateTick++
                   mapviewer.compassTick++

                   if (mapLoader.item) {
                       if (mapLoader.item.headingLineCanvas) mapLoader.item.headingLineCanvas.safeRequestPaint()
                       if (mapLoader.item.maxDoaLineCanvas)  mapLoader.item.maxDoaLineCanvas.safeRequestPaint()
                   }
               }
           }

           MouseArea {
               anchors.fill: parent
               hoverEnabled: true

               onEntered: hudBox.hoverKey = cardKey
               onExited:  if (hudBox.hoverKey === cardKey) hudBox.hoverKey = ""

               onClicked: {
                   hudBox.pinnedKey = cardKey

                   mapviewer.selectedKey = cardKey
                   var c = mapviewer.coordOfKey(cardKey)
                   if (c) mapviewer.selectedCoord = c

                   if (mapviewer.followPositionEnabled && mapLoader.item && mapLoader.item.map)
                       mapLoader.item.map.center = mapviewer.selectedCoord
               }
           }

           // ===================== COMPASS HUD =====================
           Item {
               id: compassHud
               visible: mapviewer.showCompassHud
               anchors.left: parent.left
               anchors.top: parent.top
               anchors.leftMargin: 14
               anchors.topMargin: 34
               width: hudBox.cardW - 28
               height: 112

               opacity: card.isHidden ? 0.45 : 1.0

               Text {
                   anchors.left: parent.left
                   anchors.top: parent.top
                   text: "COMPASS"
                   color: "#9aa6b2"
                   font.pixelSize: 11
                   font.bold: true
               }

               Row {
                   anchors.left: parent.left
                   anchors.top: parent.top
                   anchors.topMargin: 22
                   spacing: 14

                   Item {
                       width: 100
                       height: 100

                       Canvas {
                           id: compassMini
                           anchors.fill: parent
                           antialiasing: true

                           property real mapBearingDeg: (mapLoader.item && mapLoader.item.map) ? Number(mapLoader.item.map.bearing || 0) : 0
                           property real doaWorldDeg: (card.doaIt ? Number(card.doaIt.doaDeg || 0) : NaN)

                           function _deg2rad(d) { return Number(d) * Math.PI / 180.0 }
                           function _wrap360(d) {
                               d = Number(d); if (!isFinite(d)) return 0
                               d = d % 360; if (d < 0) d += 360
                               return d
                           }

                           onPaint: {
                               var ctx = getContext("2d")
                               ctx.clearRect(0, 0, width, height)

                               var cx = width/2, cy = height/2
                               var r  = Math.min(width, height)/2 - 3

                               ctx.beginPath()
                               ctx.arc(cx, cy, r, 0, Math.PI*2)
                               ctx.strokeStyle = "rgba(255,255,255,0.35)"
                               ctx.lineWidth = 2
                               ctx.stroke()

                               ctx.beginPath()
                               ctx.arc(cx, cy, r*0.62, 0, Math.PI*2)
                               ctx.strokeStyle = "rgba(255,255,255,0.12)"
                               ctx.lineWidth = 1
                               ctx.stroke()

                               var mb = _wrap360(mapBearingDeg)
                               var rot = _deg2rad(-mb)

                               ctx.save()
                               ctx.translate(cx, cy)
                               ctx.rotate(rot)

                               for (var a = 0; a < 360; a += 10) {
                                   var isMajor = (a % 90 === 0)
                                   var isMid   = (a % 30 === 0)
                                   var len = isMajor ? 10 : (isMid ? 7 : 4)

                                   var rad = _deg2rad(a - 90)
                                   var x1 = (r - len) * Math.cos(rad)
                                   var y1 = (r - len) * Math.sin(rad)
                                   var x2 = (r) * Math.cos(rad)
                                   var y2 = (r) * Math.sin(rad)

                                   ctx.beginPath()
                                   ctx.moveTo(x1, y1)
                                   ctx.lineTo(x2, y2)
                                   ctx.strokeStyle = isMajor ? "rgba(255,255,255,0.60)"
                                                : isMid   ? "rgba(255,255,255,0.32)"
                                                          : "rgba(255,255,255,0.18)"
                                   ctx.lineWidth = isMajor ? 2 : 1
                                   ctx.stroke()
                               }

                               ctx.fillStyle = "rgba(255,255,255,0.85)"
                               ctx.font = "bold 12px sans-serif"
                               ctx.textAlign = "center"
                               ctx.textBaseline = "middle"

                               function drawLabel(txt, deg, rr) {
                                   var rad = _deg2rad(deg - 90)
                                   ctx.fillText(txt, rr*Math.cos(rad), rr*Math.sin(rad))
                               }
                               drawLabel("N", 0,   r - 14)
                               drawLabel("E", 90,  r - 14)
                               drawLabel("S", 180, r - 14)
                               drawLabel("W", 270, r - 14)

                               ctx.restore()

                               // if hidden -> do not draw doa line
                               if (card.isHidden) {
                                   ctx.fillStyle = "rgba(255,255,255,0.20)"
                                   ctx.font = "12px sans-serif"
                                   ctx.textAlign = "center"
                                   ctx.textBaseline = "middle"
                                   ctx.fillText("OFF", cx, cy)
                               } else {
                                   var doa = doaWorldDeg
                                   if (isFinite(doa)) {
                                       doa = _wrap360(doa)
                                       var rel = _wrap360(doa - mb)
                                       var radDoa = _deg2rad(rel - 90)

                                       var L = r - 14
                                       var x = cx + L * Math.cos(radDoa)
                                       var y = cy + L * Math.sin(radDoa)

                                       var pc = String(card.pinColor || "#00c896")

                                       // glow
                                       ctx.save()
                                       ctx.globalAlpha = 0.25
                                       ctx.beginPath()
                                       ctx.moveTo(cx, cy)
                                       ctx.lineTo(x, y)
                                       ctx.strokeStyle = pc
                                       ctx.lineWidth = 8
                                       ctx.stroke()
                                       ctx.restore()

                                       // main line
                                       ctx.beginPath()
                                       ctx.moveTo(cx, cy)
                                       ctx.lineTo(x, y)
                                       ctx.strokeStyle = pc
                                       ctx.lineWidth = 3
                                       ctx.stroke()

                                       // dot
                                       ctx.beginPath()
                                       ctx.arc(x, y, 4.0, 0, Math.PI*2)
                                       ctx.fillStyle = pc
                                       ctx.fill()
                                   } else {
                                       ctx.fillStyle = "rgba(255,255,255,0.28)"
                                       ctx.font = "12px sans-serif"
                                       ctx.textAlign = "center"
                                       ctx.textBaseline = "middle"
                                       ctx.fillText("-", cx, cy)
                                   }
                               }

                               ctx.beginPath()
                               ctx.arc(cx, cy, 3.0, 0, Math.PI*2)
                               ctx.fillStyle = "rgba(255,255,255,0.55)"
                               ctx.fill()
                           }

                           Connections {
                               target: mapviewer
                               function onDoaUpdateTickChanged() {
                                   compassMini.doaWorldDeg = (card.doaIt ? Number(card.doaIt.doaDeg || 0) : NaN)
                                   compassMini.requestPaint()
                               }
                               function onCompassTickChanged() { compassMini.requestPaint() }
                               function onSelectedKeyChanged() { compassMini.requestPaint() }
                           }
                           Connections {
                               target: (mapLoader.item && mapLoader.item.map) ? mapLoader.item.map : null
                               function onBearingChanged() {
                                   compassMini.mapBearingDeg = Number((mapLoader.item && mapLoader.item.map) ? mapLoader.item.map.bearing : 0)
                                   compassMini.requestPaint()
                               }
                           }
                           Component.onCompleted: requestPaint()
                       }
                   }

                   Column {
                       spacing: 6
                       width: hudBox.cardW - 28 - 96 - 14

                       Text {
                           text: "Heading: " + mapviewer._fmtDeg(card.headingDeg)
                           color: "#e6edf3"
                           opacity: card.isHidden ? 0.55 : 1.0
                           font.pixelSize: 16
                           font.bold: true
                           elide: Text.ElideRight
                           width: parent.width
                       }

                       Text {
                           text: (card.doaIt && !card.isHidden)
                                 ? ("DoA: " + mapviewer._fmtDeg(Number(card.doaIt.doaDeg || 0)))
                                 : "DoA: -"
                           color: card.isHidden ? "#69737C" : String(card.pinColor || "#1DCD9F")
                           font.pixelSize: 14
                           font.bold: true
                           elide: Text.ElideRight
                           width: parent.width
                       }

                       Text {
                           text: (card.doaIt && !card.isHidden)
                                 ? ("Conf: " + mapviewer._fmtConf(Number(card.doaIt.confidence || 0)))
                                 : "Conf: -"
                           color: card.isHidden ? "#69737C" : "#00FFAA"
                           font.pixelSize: 13
                           font.bold: true
                           elide: Text.ElideRight
                           width: parent.width
                       }

                       Text {
                           text: (!card.isHidden && card.doaIt)
                                 ? ("Raw: " + mapviewer._fmtDeg(Number(card.doaIt.doaRawDeg || 0))
                                    + "   Map: " + mapviewer._fmtDeg(
                                           (mapLoader.item && mapLoader.item.map)
                                           ? Number(mapLoader.item.map.bearing || 0)
                                           : 0))
                                 : ("Map: " + mapviewer._fmtDeg(
                                           (mapLoader.item && mapLoader.item.map)
                                           ? Number(mapLoader.item.map.bearing || 0)
                                           : 0))
                           color: "#9aa6b2"
                           font.pixelSize: 11
                           elide: Text.ElideRight
                           width: parent.width
                       }
                   }
               }
           }
        // ===================== MAX HUD placeholder (kept) =====================
        Item {
            id: maxHud
            visible: mapviewer.showMaxDoaHud
            anchors.left: parent.left
            anchors.top: compassHud.visible ? compassHud.bottom : parent.top
            anchors.leftMargin: 14
            anchors.topMargin: compassHud.visible ? 6 : 34
            width: hudBox.cardW - 28
            height: 20

            Connections {
                target: mapviewer
                function onDoaUpdateTickChanged() { card.doaIt = mapviewer.getActiveDoaItemByKey(cardKey) }
                function onCompassTickChanged()   { card.doaIt = mapviewer.getActiveDoaItemByKey(cardKey) }
                function onSelectedKeyChanged()   { card.doaIt = mapviewer.getActiveDoaItemByKey(cardKey) }
            }
        }
    }
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
