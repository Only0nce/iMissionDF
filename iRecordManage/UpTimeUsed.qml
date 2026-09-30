// ===== UpTimeUsed.qml =====
// Phase 6 HMI: presentation-only migration. Input contract is unchanged.
import QtQuick 2.12
import "../ui"

Item {
    id: rootUpTimeUsed
    width: 400
    height: 400

    property bool darkMode: true
    property string uptimeText: ""
    property real load1: 0
    property real load5: 0
    property real load15: 0
    property int tasksTotal: 0
    property int threadsTotal: 0
    property int tasksRunning: 0
    property string updatedText: ""

    Theme { id: hmiTheme; darkMode: rootUpTimeUsed.darkMode }

    property color cCard: hmiTheme.card
    property color cBorder: hmiTheme.line
    property color cText: hmiTheme.text
    property color cMuted: hmiTheme.muted
    property color cAccent: hmiTheme.info
    property color cGreen: hmiTheme.success

    function fmt2(x){
        var n = Number(x)
        if (isNaN(n)) return "0.00"
        return n.toFixed(2)
    }

    Rectangle {
        anchors.fill: parent
        radius: hmiTheme.radiusLg
        color: rootUpTimeUsed.cCard
        border.width: 1
        border.color: rootUpTimeUsed.cBorder
        clip: true
    }

    Column {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        Text {
            text: "SYSTEM"
            color: rootUpTimeUsed.cText
            font.pixelSize: 18
            font.bold: true
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
        }

        Rectangle {
            width: parent.width
            height: 82
            radius: hmiTheme.radiusMd
            color: hmiTheme.cardAlt
            border.color: hmiTheme.line
            border.width: 1
            Column {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 4
                Text { text: "UPTIME"; color: rootUpTimeUsed.cMuted; font.pixelSize: 11; font.bold: true }
                Text { text: rootUpTimeUsed.uptimeText !== "" ? rootUpTimeUsed.uptimeText : "00:00:00"; color: rootUpTimeUsed.cGreen; font.pixelSize: 30; font.bold: true }
            }
        }

        Rectangle {
            width: parent.width
            height: 90
            radius: hmiTheme.radiusMd
            color: hmiTheme.cardAlt
            border.color: hmiTheme.line
            border.width: 1
            Column {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 6
                Text { text: "LOAD AVERAGE"; color: rootUpTimeUsed.cMuted; font.pixelSize: 11; font.bold: true }
                Row {
                    spacing: 14
                    Text { text: fmt2(rootUpTimeUsed.load1); color: rootUpTimeUsed.cAccent; font.pixelSize: 20; font.bold: true }
                    Text { text: fmt2(rootUpTimeUsed.load5); color: rootUpTimeUsed.cText; font.pixelSize: 20; font.bold: true; opacity: 0.85 }
                    Text { text: fmt2(rootUpTimeUsed.load15); color: rootUpTimeUsed.cText; font.pixelSize: 20; font.bold: true; opacity: 0.70 }
                }
                Text { text: "1m / 5m / 15m"; color: rootUpTimeUsed.cMuted; font.pixelSize: 11 }
            }
        }

        Rectangle {
            width: parent.width
            height: 82
            radius: hmiTheme.radiusMd
            color: hmiTheme.cardAlt
            border.color: hmiTheme.line
            border.width: 1
            Column {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 5
                Text { text: "TASKS / THREADS"; color: rootUpTimeUsed.cMuted; font.pixelSize: 11; font.bold: true }
                Text {
                    text: "Tasks " + rootUpTimeUsed.tasksTotal + "   •   " + rootUpTimeUsed.threadsTotal + " thr   •   " + rootUpTimeUsed.tasksRunning + " running"
                    color: rootUpTimeUsed.cText
                    font.pixelSize: 16
                    font.bold: true
                    width: parent.width
                    elide: Text.ElideRight
                }
            }
        }

        Item { width: 1; height: 1 }
        Text {
            text: rootUpTimeUsed.updatedText !== "" ? rootUpTimeUsed.updatedText : ""
            color: rootUpTimeUsed.cMuted
            font.pixelSize: 11
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            opacity: 0.9
        }
    }
}
