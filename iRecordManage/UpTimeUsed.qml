// ===== UpTimeUsed.qml (FULL FILE) =====
import QtQuick 2.12
import QtQuick.Controls 2.12

Item {
    id: rootUpTimeUsed
    width: 400
    height: 400

    // inputs
    property string uptimeText: ""
    property real load1: 0
    property real load5: 0
    property real load15: 0
    property int tasksTotal: 0
    property int threadsTotal: 0
    property int tasksRunning: 0
    property string updatedText: ""

    // theme (match CPU/RAM cards)
    property color cCard:   "#2A2F33"
    property color cBorder: "#3D6B7E"
    property color cText:   "#E7F2F7"
    property color cMuted:  "#A9B6BF"
    property color cAccent: "#3E86C8"   // blue
    property color cGreen:  "#29C36A"

    function fmt2(x){
        var n = Number(x)
        if (isNaN(n)) return "0.00"
        return n.toFixed(2)
    }

    Rectangle {
        anchors.fill: parent
        radius: 10
        color: rootUpTimeUsed.cCard
        border.width: 1
        border.color: rootUpTimeUsed.cBorder
        clip: true
    }

    Column {
        anchors.fill: parent
        anchors.margins: 22
        spacing: 14

        Text {
            text: "SYSTEM"
            color: rootUpTimeUsed.cText
            font.pixelSize: 22
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            opacity: 0.95
        }

        // UPTIME
        Rectangle {
            width: parent.width
            height: 90
            radius: 8
            color: "#23282C"
            border.color: "#121518"
            border.width: 1

            Column {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 6

                Text {
                    text: "Uptime"
                    color: rootUpTimeUsed.cMuted
                    font.pixelSize: 14
                }

                Text {
                    text: (rootUpTimeUsed.uptimeText !== "" ? rootUpTimeUsed.uptimeText : "00:00:00")
                    color: rootUpTimeUsed.cGreen
                    font.pixelSize: 34
                    font.bold: true
                }
            }
        }

        // LOAD AVG
        Rectangle {
            width: parent.width
            height: 100
            radius: 8
            color: "#23282C"
            border.color: "#121518"
            border.width: 1

            Column {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8

                Text {
                    text: "Load average"
                    color: rootUpTimeUsed.cMuted
                    font.pixelSize: 14
                }

                Row {
                    spacing: 16

                    Text { text: fmt2(rootUpTimeUsed.load1)  ; color: rootUpTimeUsed.cAccent; font.pixelSize: 22; font.bold: true }
                    Text { text: fmt2(rootUpTimeUsed.load5)  ; color: rootUpTimeUsed.cText  ; font.pixelSize: 22; font.bold: true; opacity: 0.85 }
                    Text { text: fmt2(rootUpTimeUsed.load15) ; color: rootUpTimeUsed.cText  ; font.pixelSize: 22; font.bold: true; opacity: 0.70 }
                }

                Text {
                    text: "1m / 5m / 15m"
                    color: rootUpTimeUsed.cMuted
                    font.pixelSize: 12
                }
            }
        }

        // TASKS
        Rectangle {
            width: parent.width
            height: 90
            radius: 8
            color: "#23282C"
            border.color: "#121518"
            border.width: 1

            Column {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 6

                Text {
                    text: "Tasks / Threads"
                    color: rootUpTimeUsed.cMuted
                    font.pixelSize: 14
                }

                Text {
                    text: "Tasks: " + rootUpTimeUsed.tasksTotal
                          + ",  " + rootUpTimeUsed.threadsTotal + " thr;  "
                          + rootUpTimeUsed.tasksRunning + " running"
                    color: rootUpTimeUsed.cText
                    font.pixelSize: 20
                    font.bold: true
                }
            }
        }

        // updated text
        Text {
            text: rootUpTimeUsed.updatedText !== "" ? rootUpTimeUsed.updatedText : ""
            color: rootUpTimeUsed.cMuted
            font.pixelSize: 13
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            opacity: 0.9
        }
    }
}
