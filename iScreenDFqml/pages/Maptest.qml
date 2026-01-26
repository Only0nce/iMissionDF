
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtLocation 5.15
import QtPositioning 5.15

Item {
    id: root
    width: 1920
    height: 1080

    property bool isDarkTheme: true
    property string darkStyleUrl: "http://127.0.0.1:8080/styles/dark/style.json"
    property string lightStyleUrl: "http://127.0.0.1:8080/styles/osm-bright-ifz/style.json"
    property string currentStyleUrl: isDarkTheme ? darkStyleUrl : lightStyleUrl
    property real oldLat: 0
    property real oldLon: 0
    property real oldZoom: 13
    property real oldBearing: 0

    Loader {
        id: mapLoader
        anchors.fill: parent
        sourceComponent: mapComponent
    }

    Component {
        id: mapComponent

        Item {
            id: mapWrapper
            anchors.fill: parent

            // export innerMap as "map"
            property alias map: map

            // ListModel { id: plotLinesModel }

            // function addPlotLine(lat, lon, bearing, distanceMeters = 1500) {
            //     const R = 6378137
            //     const bearingRad = bearing * Math.PI / 180
            //     const dLat = (distanceMeters * Math.cos(bearingRad)) / R
            //     const dLon = (distanceMeters * Math.sin(bearingRad)) / (R * Math.cos(lat * Math.PI / 180))
            //     const endLat = lat + dLat * 180 / Math.PI
            //     const endLon = lon + dLon * 180 / Math.PI
            //     if (plotLinesModel.count >= 200) plotLinesModel.remove(0)
            //     plotLinesModel.append({
            //         lat: lat,
            //         lon: lon,
            //         endLat: endLat,
            //         endLon: endLon,
            //         bearing: bearing,
            //         color: "#0077FF"
            //     })
            // }

            // Timer {
            //     interval: 2000; running: true; repeat: true
            //     onTriggered: {
            //         if (Krakenmapval.gpslat !== 0 && Krakenmapval.gpslong !== 0) {
            //             addPlotLine(Krakenmapval.gpslat, Krakenmapval.gpslong, Krakenmapval.degree)
            //             console.log("[Auto] Added plotLine at", Krakenmapval.gpslat, Krakenmapval.gpslong)
            //         }
            //     }
            // }

            // Repeater {
            //     model: plotLinesModel
            //     delegate: MapPolyline {
            //         line.width: 2
            //         line.color: color
            //         path: [
            //             QtPositioning.coordinate(lat, lon),
            //             QtPositioning.coordinate(endLat, endLon)
            //         ]
            //     }
            // }

            Plugin {
                id: mapboxPlugin
                name: "mapboxgl"
                PluginParameter { name: "mapboxgl.access_token"; value: "no-token-required" }
                PluginParameter { name: "mapboxgl.mapping.cache.memory"; value: "10" }
                PluginParameter { name: "mapboxgl.mapping.cache.size"; value: "10485760" }
                PluginParameter { name: "mapboxgl.mapping.transitions.fadeDuration"; value: "0" }
                PluginParameter { name: "mapboxgl.mapping.tilecache.enable"; value: "false" }
                PluginParameter { name: "mapboxgl.mapping.cache.database"; value: "off" }
                PluginParameter { name: "mapboxgl.mapping.offline_mode"; value: "true" }          // RAM cache
                PluginParameter { name: "mapboxgl.mapping.additional_style_urls"; value: currentStyleUrl }
                PluginParameter { name: "mapboxgl.mapping.additional_style_urls"; value: root.currentStyleUrl}
            }

            Map {
                id: map
                anchors.fill: parent
                plugin: mapboxPlugin
                center: QtPositioning.coordinate(13.75, 100.5)
                zoomLevel: 13
                maximumZoomLevel: 18

                Behavior on bearing {
                     NumberAnimation {
                            duration: 350
                            easing.type: Easing.InOutQuad
                        }
                    }
                // Pin ตัวอย่าง
                MapQuickItem {
                    id: pinItem
                    coordinate: QtPositioning.coordinate(Krakenmapval.gpslat, Krakenmapval.gpslong)
                    anchorPoint.x: 16; anchorPoint.y: 32
                    sourceItem: Image {
                        width: 32; height: 32
                        source: "qrc:/images/marker.png"
                    }
                }
                MapQuickItem {
                    id: doaOverlay
                    coordinate: pinItem.coordinate
                    anchorPoint.x: doaCanvas.width / 2
                    anchorPoint.y: doaCanvas.height / 2

                    sourceItem: Canvas {
                        id: doaCanvas
                        property var doaFrame: ({})
                        property real headingDeg: Krakenmapval.degree || 0
                        property real radiusInMeters: 400

                        width: 200
                        height: 200
                        antialiasing: true

                        function updateCanvasSize() {
                            const center = pinItem.coordinate
                            const earthRadius = 6378137
                            const dLng = (radiusInMeters / (earthRadius * Math.cos(Math.PI * center.latitude / 180))) * (180 / Math.PI)
                            const offsetCoord = QtPositioning.coordinate(center.latitude, center.longitude + dLng)

                            const p1 = map.fromCoordinate(center, false)
                            const p2 = map.fromCoordinate(offsetCoord, false)

                            const pxRadius = Math.abs(p2.x - p1.x)
                            const pxSize = pxRadius * 2

                            doaCanvas.width = pxSize
                            doaCanvas.height = pxSize
                        }

                        onPaint: {
                            if (!doaFrame || !doaFrame.rArray || !doaFrame.thetaArray)
                                return;

                            const ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)

                            const cx = width / 2
                            const cy = height / 2
                            const maxR = width / 2
                            const wedgeRad = 3 * Math.PI / 180

                            const rArray = doaFrame.rArray
                            const tArray = doaFrame.thetaArray

                            function normalize(v) {
                                return Math.max(0, Math.min(1, (v + 5.0) / 5.0))
                            }

                            function colorFromIntensity(i) {
                                const r = Math.round(255 * Math.max(Math.min(1.5 - Math.abs(4 * i - 3), 1), 0))
                                const g = Math.round(255 * Math.max(Math.min(1.5 - Math.abs(4 * i - 2), 1), 0))
                                const b = Math.round(255 * Math.max(Math.min(1.5 - Math.abs(4 * i - 1), 1), 0))
                                return [r, g, b]
                            }

                            // ctx.strokeStyle = "#444"
                            // ctx.lineWidth = 1
                            // ctx.setLineDash([2, 4])
                            // for (let i = 1; i <= 5; i++) {
                            //     const r = (i / 5) * maxR
                            //     ctx.beginPath()
                            //     ctx.arc(cx, cy, r, 0, 2 * Math.PI)
                            //     ctx.stroke()
                            // }
                            // ctx.setLineDash([])

                            for (let i = 0; i < rArray.length; ++i) {
                                const value = rArray[i]
                                const intensity = normalize(value)
                                const rgb = colorFromIntensity(intensity)

                                const theta = tArray[i]
                                const angleDeg = (theta + headingDeg - map.bearing - 90 + 361) % 360;
                                const angleRad = angleDeg * Math.PI / 180;
                                const radius = intensity * maxR

                                ctx.beginPath()
                                ctx.moveTo(cx, cy)
                                ctx.lineTo(cx + radius * Math.cos(angleRad - wedgeRad), cy + radius * Math.sin(angleRad - wedgeRad))
                                ctx.lineTo(cx + radius * Math.cos(angleRad + wedgeRad), cy + radius * Math.sin(angleRad + wedgeRad))
                                ctx.closePath()

                                ctx.fillStyle = `rgba(${rgb[0]},${rgb[1]},${rgb[2]},0.04)`
                                ctx.fill()
                            }

                            const headingRad = (headingDeg - map.bearing - 90 + 360) % 361 * Math.PI / 180;
                            const hx1 = cx + maxR * Math.cos(headingRad)
                            const hy1 = cy + maxR * Math.sin(headingRad)
                            ctx.beginPath()
                            ctx.moveTo(cx, cy)
                            ctx.lineTo(hx1, hy1)
                            ctx.strokeStyle = "#66FF33"
                            ctx.lineWidth = 2
                            ctx.stroke()
                        }
                        Connections {
                            target: Krakenmapval
                            function onDoaChanged() {
                                doaCanvas.doaFrame = {
                                    rArray: Krakenmapval.doaRArray,
                                    thetaArray: Krakenmapval.doaThetaArray
                                }
                                doaCanvas.requestPaint()
                            }
                            function onDegreeChanged() {
                                doaCanvas.headingDeg = Krakenmapval.degree
                                doaCanvas.requestPaint()
                            }
                        }

                        Connections {
                            target: map
                            function onZoomLevelChanged() {
                                doaCanvas.updateCanvasSize()
                                doaCanvas.requestPaint()
                            }
                            function onCenterChanged() {
                                doaCanvas.updateCanvasSize()
                                doaCanvas.requestPaint()
                            }
                            function onBearingChanged() {
                                doaCanvas.updateCanvasSize()
                                doaCanvas.requestPaint()
                            }
                        }

                        Component.onCompleted: {
                            doaCanvas.doaFrame = {
                                rArray: Krakenmapval.doaRArray,
                                thetaArray: Krakenmapval.doaThetaArray
                            }
                            updateCanvasSize()
                            requestPaint()
                    }
                }
            }
            }

            Canvas {
                id: maxDoaLineCanvas
                anchors.fill: parent
                z: 9999
                antialiasing: true

                property var rArray: []
                property var thetaArray: []
                property int doaMaxIndex: -1
                property real headingDeg: Krakenmapval.degree || 0
                property bool visibleLine: true


                onPaint: {
                    if (!visibleLine || rArray.length === 0 || thetaArray.length === 0 || doaMaxIndex < 0 || doaMaxIndex >= thetaArray.length)
                        return

                    const ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)

                    const center = map.fromCoordinate(pinItem.coordinate, false)
                    const cx = center.x
                    const cy = center.y
                    const maxR = Math.max(width, height)

                    const peakTheta = maxDoaLineCanvas.doaMaxIndex
                    const heading = headingDeg
                    const bearing = map.bearing

                    const peakAngleDeg = (peakTheta + heading - bearing - 90 + 360) % 360
                    const peakAngleRad = peakAngleDeg * Math.PI / 180
                    // const peakAngleRad = (maxDoaLineCanvas.doaMaxIndex + heading) % 360

                    // maxDoaLineCanvas.doaMaxIndex

                    const px = cx + maxR * Math.cos(peakAngleRad)
                    const py = cy + maxR * Math.sin(peakAngleRad)

                    ctx.beginPath()
                    ctx.moveTo(cx, cy)
                    ctx.lineTo(px, py)
                    ctx.strokeStyle = "#1DCD9F"
                    ctx.lineWidth = 2
                    ctx.stroke()

                    // Optional debug log
                    // console.log("DOA MAX", {
                    //     peakTheta, heading, bearing, peakAngleDeg
                    // })
                }

                Connections {
                    target: Krakenmapval
                    function onDoaChanged() {
                        maxDoaLineCanvas.rArray = Krakenmapval.doaRArray
                        maxDoaLineCanvas.thetaArray = Krakenmapval.doaThetaArray
                        maxDoaLineCanvas.doaMaxIndex = Krakenmapval.doa_max
                        maxDoaLineCanvas.requestPaint()
                    }
                    function onDegreeChanged() {
                        maxDoaLineCanvas.headingDeg = Krakenmapval.degree
                        maxDoaLineCanvas.requestPaint()
                    }
                }

                Connections {
                    target: map
                    function onZoomLevelChanged() { maxDoaLineCanvas.requestPaint() }
                    function onCenterChanged() { maxDoaLineCanvas.requestPaint() }
                    function onBearingChanged() { maxDoaLineCanvas.requestPaint() }
                }

                Component.onCompleted: requestPaint()
            }


            // ปุ่มซูม
            Column {
                anchors.rightMargin: 30
                anchors.bottomMargin: 50
                spacing: 12
                anchors {
                    right: parent.right
                    bottom: parent.bottom
                    margins: 24
                }

                Button {
                    width: 48; height: 48
                    onClicked: if (map.zoomLevel < map.maximumZoomLevel) map.zoomLevel += 1
                    contentItem: Text {
                        text: "+"; font.pixelSize: 24
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.centerIn: parent
                        color: "white"
                    }
                    background: Rectangle { color: "#169976"; radius: width / 2 }
                }

                Button {
                    width: 48; height: 48
                    onClicked: if (map.zoomLevel > map.minimumZoomLevel) map.zoomLevel -= 1
                    contentItem: Text {
                        text: "-"; font.pixelSize: 24
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.centerIn: parent
                        color: "white"
                    }
                    background: Rectangle { color: "#169976"; radius: width / 2 }
                }
            }

            // ปุ่ม Reset North
            Item {
                id: _item1
                width: 48; height: 48
                anchors.bottom: parent.bottom
                anchors.rightMargin: 30
                anchors.bottomMargin: 290
                anchors.right: parent.right

                Rectangle {
                    id: rectangle2
                    radius: width / 2
                    anchors.fill: parent
                    color: "#C7C8CC"; opacity: 0.6
                    MouseArea {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 0
                        anchors.rightMargin: 0
                        anchors.topMargin: 0
                        anchors.bottomMargin: 0
                        onClicked: map.bearing = 0
                    }
                }

                Image {
                    id: northArrow
                    anchors.centerIn: parent
                    width: 32; height: 32
                    source: "qrc:/images/cardinal-point.png"
                    transformOrigin: Item.Center

                    // Smooth rotation logic
                    property real smoothRotation: -map.bearing

                    RotationAnimator on rotation {
                        duration: 300
                        easing.type: Easing.InOutQuad
                    }

                    onSmoothRotationChanged: {
                        // Update rotation only when bearing changes
                        northArrow.rotation = smoothRotation
                    }
                }
            }


            Item {
                id: navButtonItem
                width: 48
                height: 48
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.bottomMargin: 170
                anchors.rightMargin: 30

                property bool navigationEnabled: false
                property real previousBearing: 0

                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: navButtonItem.navigationEnabled ? "#3498db" : "#C7C8CC"
                    opacity: 0.6

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            navButtonItem.navigationEnabled = !navButtonItem.navigationEnabled

                            if (!navButtonItem.navigationEnabled) {
                                map.bearing = 0
                            }
                        }
                    }
                }

                Image {
                    id: navigationarrow
                    anchors.centerIn: parent
                    width: 32
                    height: 32
                    source: navButtonItem.navigationEnabled
                        ? "qrc:/images/gps.png"
                        : "qrc:/images/disable.png"
                }

                Connections {
                    target: Krakenmapval
                    function onDegreeChanged() {
                        if (!navButtonItem.navigationEnabled)
                            return

                        let current = map.bearing % 360
                        let target = Krakenmapval.degree % 360

                        // ✅ หาค่าผิดต่างแบบ shortest path (ไม่กระตุก)
                        let diff = (target - current + 540) % 360 - 180
                        let finalBearing = current + diff

                        // ✅ ตั้งเฉพาะเมื่อเปลี่ยนจริง
                        if (Math.abs(diff) > 0.5) {
                            map.bearing = finalBearing
                            navButtonItem.previousBearing = finalBearing
                        }
                    }
                }
            }

            Item {
                id: followButton
                width: 48
                height: 48
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.bottomMargin: 230 //350
                anchors.rightMargin: 30

                property bool followPositionEnabled: false

                Rectangle {
                    radius: width / 2
                    anchors.fill: parent
                    color: followButton.followPositionEnabled ? "#3498db" : "#C7C8CC"
                    opacity: 0.6

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            followButton.followPositionEnabled = !followButton.followPositionEnabled
                            if (followButton.followPositionEnabled) {
                                map.center = QtPositioning.coordinate(Krakenmapval.gpslat, Krakenmapval.gpslong)
                            }
                        }
                    }
                }

                Image {
                    anchors.centerIn: parent
                    width: 28
                    height: 28
                    source: followButton.followPositionEnabled
                        ? "qrc:/images/pin-map.png"
                        : "qrc:/images/pin_disable.png"
                }

                Connections {
                    target: Krakenmapval
                    function onGpslatChanged() {
                        if (followButton.followPositionEnabled)
                            map.center = QtPositioning.coordinate(Krakenmapval.gpslat, Krakenmapval.gpslong)
                    }
                    function onGpslongChanged() {
                        if (followButton.followPositionEnabled)
                            map.center = QtPositioning.coordinate(Krakenmapval.gpslat, Krakenmapval.gpslong)
                    }
                }
            }

        }
    }

    // ปุ่มเปลี่ยนธีม
    Item {
        id: themeButton
        width: 48
        height: 48
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.bottomMargin: 350
        anchors.rightMargin: 30

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "#C7C8CC"
            opacity: 0.6

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    isDarkTheme = !isDarkTheme

                    if (mapLoader.item && mapLoader.item.map) {
                        oldLat = mapLoader.item.map.center.latitude
                        oldLon = mapLoader.item.map.center.longitude
                        oldZoom = mapLoader.item.map.zoomLevel
                        oldBearing = mapLoader.item.map.bearing
                    }

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
            source: isDarkTheme
                ? "qrc:/images/moon-and-stars.png"
                : "qrc:/images/sun_theme.png"
        }
    }

    Rectangle {
        id: rectangle
        x: 0
        y: 1048
        width: 1920
        height: 32
        color: "#000000"
    }
}
