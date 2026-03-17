// TumblerDateTime.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: rootTumblerDateTime
    width: 900
    height: 420

    /* ===================== API ===================== */
    function pad2(n) { n = Number(n); return (n < 10 ? "0" : "") + n }

    function fromText() { return fy + "-" + pad2(fm) + "-" + pad2(fd) + " " + pad2(fh) + ":" + pad2(fmin) }
    function toText()   { return ty + "-" + pad2(tm) + "-" + pad2(td) + " " + pad2(th) + ":" + pad2(tmin) }

    // Reset helpers
    function setFromNow() {
        var d = new Date()
        fy = d.getFullYear()
        fm = d.getMonth() + 1
        fd = d.getDate()
        fh = d.getHours()
        fmin = d.getMinutes()
        syncFromAll()
    }

    function setToNow() {
        var d = new Date()
        ty = d.getFullYear()
        tm = d.getMonth() + 1
        td = d.getDate()
        th = d.getHours()
        tmin = d.getMinutes()
        syncToAll()
    }

    function setTodayAll() {
        setFromNow()
        setToNow()
    }

    /* ===================== DATA ===================== */
    // FROM
    property int fy: 2000
    property int fm: 1
    property int fd: 1
    property int fh: 0
    property int fmin: 0
    property var startDays: []

    // TO
    property int ty: 2000
    property int tm: 1
    property int td: 1
    property int th: 0
    property int tmin: 0
    property var endDays: []

    // models
    property var years: []
    property var months: [1,2,3,4,5,6,7,8,9,10,11,12]
    property var hours: []
    property var mins: []

    function initModels() {
        var a = []
        for (var y = 2000; y <= 2100; ++y) a.push(y)
        years = a

        a = []
        for (var i = 0; i < 24; ++i) a.push(i)
        hours = a

        a = []
        for (var m = 0; m < 60; ++m) a.push(m)
        mins = a
    }

    function daysInMonth(y, m) {
        if (m === 1 || m === 3 || m === 5 || m === 7 || m === 8 || m === 10 || m === 12) return 31
        if (m === 4 || m === 6 || m === 9 || m === 11) return 30
        var leap = ((y % 4 === 0 && y % 100 !== 0) || (y % 400 === 0))
        return leap ? 29 : 28
    }

    function rebuildStartDays() {
        var max = daysInMonth(fy, fm)
        var arr = []
        for (var d = 1; d <= max; ++d) arr.push(d)
        startDays = arr

        if (fd < 1) fd = 1
        if (fd > max) fd = max

        if (tumblerDay) {
            var idx = startDays.indexOf(fd)
            tumblerDay.currentIndex = (idx >= 0 ? idx : 0)
        }
    }

    function rebuildEndDays() {
        var max = daysInMonth(ty, tm)
        var arr = []
        for (var d = 1; d <= max; ++d) arr.push(d)
        endDays = arr

        if (td < 1) td = 1
        if (td > max) td = max

        if (tumblerDayEnd) {
            var idx = endDays.indexOf(td)
            tumblerDayEnd.currentIndex = (idx >= 0 ? idx : 0)
        }
    }

    function syncFromAll() {
        // year/month affect days
        if (tumblerYear)  tumblerYear.currentIndex  = Math.max(0, years.indexOf(fy))
        if (tumblerMonth) tumblerMonth.currentIndex = Math.max(0, months.indexOf(fm))
        rebuildStartDays()
        if (tumblerHour)  tumblerHour.currentIndex  = Math.max(0, hours.indexOf(fh))
        if (tumblerMin)   tumblerMin.currentIndex   = Math.max(0, mins.indexOf(fmin))
    }

    function syncToAll() {
        if (tumblerYearEnd)  tumblerYearEnd.currentIndex  = Math.max(0, years.indexOf(ty))
        if (tumblerMonthEnd) tumblerMonthEnd.currentIndex = Math.max(0, months.indexOf(tm))
        rebuildEndDays()
        if (tumblerHourEnd)  tumblerHourEnd.currentIndex  = Math.max(0, hours.indexOf(th))
        if (tumblerMinEnd)   tumblerMinEnd.currentIndex   = Math.max(0, mins.indexOf(tmin))
    }

    Component.onCompleted: {
        initModels()
        setTodayAll() // ✅ เริ่มที่เวลาปัจจุบันทันที
    }

    /* ===================== STYLE ===================== */
    Component {
        id: slotDelegate
        Label {
            width: 90
            height: 36
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: (Number(modelData) < 10 ? "0" : "") + modelData
            color: (Tumbler.tumbler.currentIndex === index) ? "white" : "#7f97a3"
            font.pixelSize: (Tumbler.tumbler.currentIndex === index) ? 18 : 14
            font.bold: (Tumbler.tumbler.currentIndex === index)
            opacity: 1.0 - Math.abs(Tumbler.displacement) / 3
        }
    }

    /* ===================== UI ===================== */
    Rectangle {
        radius: 14
        color: "#0B1216"
        border.color: "#2A3A44"
        border.width: 1
        anchors.fill: parent

        // ===== Top actions =====
        Row {
            x: 12
            y: 10
            spacing: 10

            Button {
                text: "Today"
                onClicked: setTodayAll()
            }
            Button {
                text: "Reset Start"
                onClicked: setFromNow()
            }
            Button {
                text: "Reset End"
                onClicked: setToNow()
            }

            // preview text
            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: "#9FB0BA"
                font.pixelSize: 12
                text: "From: " + fromText() + "   |   To: " + toText()
            }
        }

        // ---------------- FROM ----------------
        Rectangle {
            id: recBegin
            x: 8
            y: 48
            width: parent.width - 16
            height: 175
            radius: 10
            color: "#0F1A20"
            border.color: "#20303A"

            Text { x: 8; y: 8; text: "From"; color: "#9FB0BA"; font.pixelSize: 12 }

            RowLayout {
                x: 8; y: 29
                width: parent.width - 16
                height: 134
                spacing: 18

                ColumnLayout {
                    Layout.preferredWidth: 520
                    Layout.preferredHeight: 134
                    spacing: 6
                    property int wYear: 160
                    property int wMonth: 160
                    property int wDay: 160

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 22
                        spacing: 10
                        Rectangle { Layout.preferredWidth: wYear; Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"; border.color: "#002a9cff"; border.width: 1
                            Text { anchors.centerIn: parent; text: "Year"; color: "white"; font.pixelSize: 12 } }
                        Rectangle { Layout.preferredWidth: wMonth; Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"; border.color: "#002a9cff"; border.width: 1
                            Text { anchors.centerIn: parent; text: "Month"; color: "white"; font.pixelSize: 12 } }
                        Rectangle { Layout.preferredWidth: wDay; Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"; border.color: "#002a9cff"; border.width: 1
                            Text { anchors.centerIn: parent; text: "Day"; color: "white"; font.pixelSize: 12 } }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 125
                        spacing: 10

                        Tumbler {
                            id: tumblerYear
                            Layout.preferredWidth: wYear
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visibleItemCount: 3
                            model: years
                            delegate: slotDelegate
                            Layout.topMargin: 8
                            onCurrentIndexChanged: {
                                if (currentIndex < 0 || currentIndex >= years.length) return
                                fy = years[currentIndex]
                                rebuildStartDays()
                            }
                        }

                        Tumbler {
                            id: tumblerMonth
                            Layout.preferredWidth: wMonth
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visibleItemCount: 3
                            model: months
                            delegate: slotDelegate
                            Layout.topMargin: 8
                            onCurrentIndexChanged: {
                                if (currentIndex < 0 || currentIndex >= months.length) return
                                fm = months[currentIndex]
                                rebuildStartDays()
                            }
                        }

                        Tumbler {
                            id: tumblerDay
                            Layout.preferredWidth: wDay
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visibleItemCount: 3
                            model: startDays
                            delegate: slotDelegate
                            Layout.topMargin: 8
                            onCurrentIndexChanged: {
                                if (currentIndex < 0 || currentIndex >= startDays.length) return
                                fd = startDays[currentIndex]
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.preferredWidth: 320
                    Layout.preferredHeight: 134
                    spacing: 6
                    property int wHour: 140
                    property int wMin: 140

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 22
                        spacing: 10
                        Rectangle { Layout.preferredWidth: wHour; Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"; border.color: "#002a9cff"; border.width: 1
                            Text { anchors.centerIn: parent; text: "Hour"; color: "white"; font.pixelSize: 12 } }
                        Rectangle { Layout.preferredWidth: wMin; Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"; border.color: "#002a9cff"; border.width: 1
                            Text { anchors.centerIn: parent; text: "Min"; color: "white"; font.pixelSize: 12 } }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 125
                        spacing: 10

                        Tumbler {
                            id: tumblerHour
                            Layout.preferredWidth: wHour
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visibleItemCount: 3
                            model: hours
                            delegate: slotDelegate
                            Layout.topMargin: 8
                            onCurrentIndexChanged: {
                                if (currentIndex < 0 || currentIndex >= hours.length) return
                                fh = hours[currentIndex]
                            }
                        }

                        Tumbler {
                            id: tumblerMin
                            Layout.preferredWidth: wMin
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visibleItemCount: 3
                            model: mins
                            delegate: slotDelegate
                            Layout.topMargin: 8
                            onCurrentIndexChanged: {
                                if (currentIndex < 0 || currentIndex >= mins.length) return
                                fmin = mins[currentIndex]
                            }
                        }
                    }
                }
            }
        }

        // ---------------- TO ----------------
        Rectangle {
            id: recEnd
            x: 8
            y: recBegin.y + recBegin.height + 10
            width: parent.width - 16
            height: 175
            radius: 10
            color: "#0F1A20"
            border.color: "#20303A"

            Text { x: 8; y: 8; text: "To"; color: "#9FB0BA"; font.pixelSize: 12 }

            RowLayout {
                x: 8; y: 29
                width: parent.width - 16
                height: 134
                spacing: 18

                ColumnLayout {
                    Layout.preferredWidth: 520
                    Layout.preferredHeight: 134
                    spacing: 6
                    property int wYear: 160
                    property int wMonth: 160
                    property int wDay: 160

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 22
                        spacing: 10
                        Rectangle { Layout.preferredWidth: wYear; Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"; border.color: "#002a9cff"; border.width: 1
                            Text { anchors.centerIn: parent; text: "Year"; color: "white"; font.pixelSize: 12 } }
                        Rectangle { Layout.preferredWidth: wMonth; Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"; border.color: "#002a9cff"; border.width: 1
                            Text { anchors.centerIn: parent; text: "Month"; color: "white"; font.pixelSize: 12 } }
                        Rectangle { Layout.preferredWidth: wDay; Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"; border.color: "#002a9cff"; border.width: 1
                            Text { anchors.centerIn: parent; text: "Day"; color: "white"; font.pixelSize: 12 } }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 125
                        spacing: 10

                        Tumbler {
                            id: tumblerYearEnd
                            Layout.preferredWidth: wYear
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visibleItemCount: 3
                            model: years
                            delegate: slotDelegate
                            Layout.topMargin: 8
                            onCurrentIndexChanged: {
                                if (currentIndex < 0 || currentIndex >= years.length) return
                                ty = years[currentIndex]
                                rebuildEndDays()
                            }
                        }

                        Tumbler {
                            id: tumblerMonthEnd
                            Layout.preferredWidth: wMonth
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visibleItemCount: 3
                            model: months
                            delegate: slotDelegate
                            Layout.topMargin: 8
                            onCurrentIndexChanged: {
                                if (currentIndex < 0 || currentIndex >= months.length) return
                                tm = months[currentIndex]
                                rebuildEndDays()
                            }
                        }

                        Tumbler {
                            id: tumblerDayEnd
                            Layout.preferredWidth: wDay
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visibleItemCount: 3
                            model: endDays
                            delegate: slotDelegate
                            Layout.topMargin: 8
                            onCurrentIndexChanged: {
                                if (currentIndex < 0 || currentIndex >= endDays.length) return
                                td = endDays[currentIndex]
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.preferredWidth: 320
                    Layout.preferredHeight: 134
                    spacing: 6
                    property int wHour: 140
                    property int wMin: 140

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 22
                        spacing: 10
                        Rectangle { Layout.preferredWidth: wHour; Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"; border.color: "#002a9cff"; border.width: 1
                            Text { anchors.centerIn: parent; text: "Hour"; color: "white"; font.pixelSize: 12 } }
                        Rectangle { Layout.preferredWidth: wMin; Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"; border.color: "#002a9cff"; border.width: 1
                            Text { anchors.centerIn: parent; text: "Min"; color: "white"; font.pixelSize: 12 } }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 125
                        spacing: 10

                        Tumbler {
                            id: tumblerHourEnd
                            Layout.preferredWidth: wHour
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visibleItemCount: 3
                            model: hours
                            delegate: slotDelegate
                            Layout.topMargin: 8
                            onCurrentIndexChanged: {
                                if (currentIndex < 0 || currentIndex >= hours.length) return
                                th = hours[currentIndex]
                            }
                        }

                        Tumbler {
                            id: tumblerMinEnd
                            Layout.preferredWidth: wMin
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visibleItemCount: 3
                            model: mins
                            delegate: slotDelegate
                            Layout.topMargin: 8
                            onCurrentIndexChanged: {
                                if (currentIndex < 0 || currentIndex >= mins.length) return
                                tmin = mins[currentIndex]
                            }
                        }
                    }
                }
            }
        }
    }
}
