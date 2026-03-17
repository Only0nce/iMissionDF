// CalendarPopup.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3

Item {
    id: root
    width: 700
    height: 500

    /* ====== STATE ====== */
    readonly property var monthNames: ["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"]

    // ให้ภายนอกส่งค่าเริ่มต้นมาได้
    property date initialDate: new Date()

    // วันที่–เวลา ที่ถูกเลือก (เริ่มจาก initialDate)
    property int selYear:   initialDate.getFullYear()
    property int selMonth:  initialDate.getMonth()     // 0..11
    property int selDay:    initialDate.getDate()      // 1..31

    // ใช้ชั่วโมง 24 ชม. ตรง ๆ (0..23) และ AM/PM จะ “ตามชั่วโมง” อัตโนมัติ
    property int selHour24: initialDate.getHours()     // 0..23
    property int selMin:    initialDate.getMinutes()
    property int selSec:    initialDate.getSeconds()
    property string selAmPm: (selHour24 >= 12 ? "PM" : "AM")

    // แจ้งผลออกไปแบบสตริง => "yyyy/MM/dd HH:mm:ss"
    signal accepted(string ymdHMS)
    signal canceled()

    function pad2(n){ return (n<10?"0":"")+n }
    function toH24(){ return selHour24 }  // เราเก็บเป็น 24 ชม. อยู่แล้ว

    // ตั้งชั่วโมงแล้วอัปเดต AM/PM ให้ตรงอัตโนมัติ
    function setHour24(h) {
        selHour24 = Math.max(0, Math.min(23, h|0))
        selAmPm = (selHour24 >= 12 ? "PM" : "AM")
    }

    // เมื่อผู้ใช้สลับ AM/PM ให้แปลง hour เดิมเป็นฝั่งนั้น โดยคง “ชั่วโมงบนหน้าปัด” (0–11) ไว้
    function setAmPm(ampm) {
        var hour12 = selHour24 % 12              // 0..11
        if (ampm === "PM") setHour24(hour12 + 12)
        else               setHour24(hour12)     // AM = 0..11
    }

    function daysInMonth(y, m) { return new Date(y, m+1, 0).getDate() }
    function firstDayOfWeek(y, m) { return new Date(y, m, 1).getDay() } // 0..6 Sun..Sat

    /* ====== BACK PANEL ====== */
    Rectangle {
        id: rectangle
        anchors.fill: parent
        color: "#0e1116"
        radius: 10
        border.color: "#ffffff"
    }

    /* ====== TITLE ====== */
    Text {
        id: title
        text: qsTr("Calendar")
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 40
        font.pixelSize: 20
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: "#ffffff"
    }

    /* ====== TOP ROW (Month / Year / Enter) ====== */
    RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.topMargin: 57
        height: 40
        spacing: 16

        ComboBox {
            id: comboBoxMonth
            Layout.preferredWidth: 173
            Layout.preferredHeight: 40
            model: monthNames
            currentIndex: selMonth
            onActivated: (i)=> selMonth = i
        }

        Label {
            id: years
            text: qsTr("Year")
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pointSize: 16
            Layout.fillHeight: true
            Layout.fillWidth: false
        }

        SpinBox {
            id: spinBoxYear
            Layout.preferredWidth: 216
            Layout.preferredHeight: 40
            from: selYear - 100
            to:   selYear + 100
            value: selYear
            // ป้องกัน “2,025”
            textFromValue: function(v){ return v.toString() }
            valueFromText: function(t){ return parseInt(t) }
            onValueModified: selYear = value
        }


        //        Button {
        //            id: buttonEnterTop
        //            text: qsTr("Enter")
        //            Layout.preferredWidth: 165
        //            Layout.preferredHeight: 40
        //            onClicked: {
        //                const h24 = toH24()
        //                const ymd = selYear + "/" + pad2(selMonth+1) + "/" + pad2(selDay)
        //                const hms = pad2(h24) + ":" + pad2(selMin) + ":" + pad2(selSec)
        //                root.accepted(ymd + " " + hms)
        //            }
        //        }
    }

    /* ====== CALENDAR AREA ====== */
    Rectangle {
        id: rectangleCalendarDate
        color: "#ffffff"
        radius: 10
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.topMargin: 108
        anchors.bottomMargin: 72

        // หัวคอลัมน์ Sun..Sat
        Row {
            id: dowRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            height: 28
            spacing: 0

            Repeater {
                model: ["Sun","Mon","Tue","Wed","Thu","Fri","Sat"]
                delegate: Rectangle {
                    width: (dowRow.width) / 7
                    height: dowRow.height
                    color: "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: "#2b2b2b"
                        font.pixelSize: 14
                        font.bold: true
                    }
                }
            }
        }

        // ตาราง 7×6 (42 ช่อง)
        Grid {
            id: grid
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: dowRow.bottom
            anchors.bottom: parent.bottom
            anchors.margins: 8
            columns: 7
            rowSpacing: 4
            columnSpacing: 4

            Repeater {
                id: dayRepeater
                model: 42

                readonly property int firstDow: firstDayOfWeek(selYear, selMonth) // 0..6
                readonly property int dim: daysInMonth(selYear, selMonth)
                // วันของเดือนก่อนหน้า
                readonly property int prevMonth: (selMonth === 0 ? 11 : selMonth - 1)
                readonly property int prevYear:  (selMonth === 0 ? (selYear - 1) : selYear)
                readonly property int prevDim: daysInMonth(prevYear, prevMonth)

                delegate: Item {
                    width: (grid.width - (grid.columns-1)*grid.columnSpacing) / grid.columns
                    height: (grid.height - 5*grid.rowSpacing) / 6

                    readonly property int cellIndex: index
                    readonly property int d0: cellIndex - dayRepeater.firstDow + 1
                    readonly property bool inCurrent: d0 >= 1 && d0 <= dayRepeater.dim
                    readonly property int shownDay: inCurrent ? d0
                                                              : (d0 < 1 ? (dayRepeater.prevDim + d0) : (d0 - dayRepeater.dim))
                    readonly property int cellMonth: (inCurrent ? selMonth : (d0<1 ? dayRepeater.prevMonth : ((selMonth+1)%12)))
                    readonly property int cellYear:  (function(){
                        if (inCurrent) return selYear
                        if (d0 < 1)  return dayRepeater.prevYear
                        return (selMonth===11 ? selYear+1 : selYear)
                    })()
                    readonly property bool isSelected: (cellYear===selYear && cellMonth===selMonth && shownDay===selDay)

                    // กรอบ selection
                    Rectangle {
                        anchors.fill: parent
                        radius: 6
                        color: "transparent"
                        border.width: isSelected ? 2 : 0
                        border.color: "black"
                    }

                    // ตัวเลขวัน
                    Text {
                        anchors.centerIn: parent
                        text: shownDay
                        font.pixelSize: 16
                        font.bold: isSelected
                        color: inCurrent ? "#111111" : "#8aa0b5"
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            selYear  = cellYear
                            selMonth = cellMonth
                            selDay   = shownDay
                            comboBoxMonth.currentIndex = selMonth
                            spinBoxYear.value = selYear
                        }
                    }
                }
            }
        }
    }

    /* ====== BOTTOM BAR ====== */
    RowLayout {
        y: 434
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.bottomMargin: 8
        height: 58
        spacing: 12

        Button {
            id: buttonReset
            text: qsTr("Reset")
            Layout.preferredWidth: 93
            Layout.preferredHeight: 32
            onClicked: {
                const d = new Date()
                selYear  = d.getFullYear()
                selMonth = d.getMonth()
                selDay   = d.getDate()
                setHour24(d.getHours())   // << ใช้ฟังก์ชันนี้เพื่อ sync AM/PM อัตโนมัติ
                selMin   = d.getMinutes()
                selSec   = d.getSeconds()
                comboBoxMonth.currentIndex = selMonth
                spinBoxYear.value = selYear
            }
        }

        Button {
            id: buttonToday
            text: qsTr("Today")
            Layout.preferredWidth: 93
            Layout.preferredHeight: 32
            onClicked: {
                const d = new Date()
                selYear  = d.getFullYear()
                selMonth = d.getMonth()
                selDay   = d.getDate()
                comboBoxMonth.currentIndex = selMonth
                spinBoxYear.value = selYear
            }
        }

        Item { Layout.preferredWidth: 8 }

        // HH (00..23) — เลือกแล้ว AM/PM จะเปลี่ยนเอง
        Text { text: "HH:"; color: "white"; font.pixelSize: 16; font.bold: true; verticalAlignment: Text.AlignVCenter }
        ComboBox {
            id: comboBoxHour
            Layout.preferredWidth: 87
            Layout.preferredHeight: 32
            model: Array.from({length:24}, (_,i)=> (i<10?"0":"")+i ) // "00".."23"
            currentIndex: selHour24
            onActivated: function(i){ setHour24(i) }                 // << auto AM/PM
        }

        // mm
        Text { text: "mm:"; color: "white"; font.pixelSize: 16; font.bold: true; verticalAlignment: Text.AlignVCenter }
        ComboBox {
            id: comboBoxMinute
            Layout.preferredWidth: 91
            Layout.preferredHeight: 32
            model: Array.from({length:60}, (_,i)=> (i<10?"0":"")+i )
            currentIndex: selMin
            onActivated: (i)=> selMin = i
        }

        // ss
        Text { text: "ss:"; color: "white"; font.pixelSize: 16; font.bold: true; verticalAlignment: Text.AlignVCenter }
        ComboBox {
            id: comboBoxSecond
            Layout.preferredWidth: 84
            Layout.preferredHeight: 32
            model: Array.from({length:60}, (_,i)=> (i<10?"0":"")+i )
            currentIndex: selSec
            onActivated: (i)=> selSec = i
        }

        // AM/PM (สลับแล้วแปลงชั่วโมง 24 ชม. ให้เอง)
        ComboBox {
            id: comboBoxAMPM
            Layout.preferredWidth: 86
            Layout.preferredHeight: 32
            model: ["AM","PM"]
            currentIndex: (selAmPm==="PM"?1:0)
            onActivated: function(i){ setAmPm(i===1?"PM":"AM") }   // << keep hour12, flip half-day
        }

        Item { Layout.fillWidth: true }

        Button {
            id: buttonEnter
            text: qsTr("Enter")
            Layout.preferredWidth: 165
            Layout.preferredHeight: 40
            onClicked: {
                const h24 = toH24()
                const ymd = selYear + "/" + pad2(selMonth+1) + "/" + pad2(selDay)
                const hms = pad2(h24) + ":" + pad2(selMin) + ":" + pad2(selSec)
                root.accepted(ymd + " " + hms)
            }
        }
    }
}
