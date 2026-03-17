// ===== MonitorDisplay.qml (FULL - fixed layout) =====
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Item {
    id: rootmonitorView
    width: 1920
    height: 1080

    // ===== bind from main.qml (ตามที่คุณต้องการ) =====
    property real cpuPct: monitorCpuPct
    property string updatedText: monitorTs    // ใช้ ts จาก main.qml เป็น Updated text

    property real memUsedMB: monitorMemUsed
    property real memTotalMB: monitorMemTotal
    property real percentRAMUsed: monitorPercentRAM

    property real storageUsedGB: monitorStorageUsed
    property real storageTotalGB: monitorStorageTotal

    // ✅ TOP (uptime/load/tasks)
    property string hTopUptimeText: monitorUptimeText
    property real hTopLoad1: monitorLoad1
    property real hTopLoad5: monitorLoad5
    property real hTopLoad15: monitorLoad15
    property int hTopTasksTotal: monitorTasksTotal
    property int hTopThreadsTotal: monitorThreadsTotal
    property int hTopTasksRunning: monitorTasksRunning

    // ===== Theme =====
    property color cBg:     "#234654"
    property color cPanel:  "#1F3E4D"
    property color cBorder2:"#62A7C2"
    property color cText:   "#E7F2F7"
    property color cMuted:  "#9EC6D6"

    Rectangle { anchors.fill: parent; anchors.topMargin: 30; color: "#000000" }

    Rectangle {
        id: frame
        anchors.fill: parent
        anchors.margins: 16
        anchors.topMargin: 50
        color: "#23404d"
        border.color: rootmonitorView.cBorder2
        border.width: 1
        radius: 2

        ColumnLayout {
            id: mainCol
            anchors.fill: parent
            anchors.margins: 14
            anchors.topMargin: 30
            spacing: 12

            // ===== Header =====
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Text {
                    text: "SYSTEM MONITOR"
                    color: rootmonitorView.cText
                    font.pixelSize: 18
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#3D6B7E"
                    opacity: 0.8
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    text: rootmonitorView.updatedText !== "" ? ("Updated: " + rootmonitorView.updatedText) : ""
                    color: rootmonitorView.cMuted
                    font.pixelSize: 12
                    Layout.alignment: Qt.AlignVCenter
                }
            }

            // ===== Content Area =====
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 12

                // ===== Row 1: CPU / RAM / Storage =====
                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 442
                    spacing: 12

                    CPUused {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 548

                        valuePct: rootmonitorView.cpuPct
                        updatedText: rootmonitorView.updatedText
                    }

                    RAMused {
                        id: rAMused
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 548

                        usedMB:  rootmonitorView.memUsedMB
                        totalMB: rootmonitorView.memTotalMB
                        valuePct: rootmonitorView.percentRAMUsed
                        updatedText: rootmonitorView.updatedText
                    }

                    StorageUsed {
                        id: rootStorageUsed
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 548

                        usedGB: rootmonitorView.storageUsedGB
                        totalGB: rootmonitorView.storageTotalGB
                        valuePct: (rootmonitorView.storageTotalGB > 0)
                                  ? (rootmonitorView.storageUsedGB * 100.0 / rootmonitorView.storageTotalGB)
                                  : 0
                        updatedText: rootmonitorView.updatedText
                    }
                }

                // ===== Row 2: UpTime + Diagnostic + Empty =====
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12

                    UpTimeUsed {
                        id: rootUpTimeUsed
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 548

                        uptimeText: rootmonitorView.hTopUptimeText
                        load1: rootmonitorView.hTopLoad1
                        load5: rootmonitorView.hTopLoad5
                        load15: rootmonitorView.hTopLoad15
                        tasksTotal: rootmonitorView.hTopTasksTotal
                        threadsTotal: rootmonitorView.hTopThreadsTotal
                        tasksRunning: rootmonitorView.hTopTasksRunning
                        updatedText: rootmonitorView.updatedText
                    }

                    // Diagnostic panel
                    Rectangle {
                        id: diagPanel
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 548

                        color: rootmonitorView.cPanel
                        radius: 10
                        border.color: "#2E6E86"
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 12

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 70
                                color: "#eaf4fa"
                                radius: 8
                                border.color: "#000000"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: qsTr("Diagnostic")
                                    font.pixelSize: 28
                                    font.bold: true
                                    color: "#0b0f12"
                                }
                            }

                            Button {
                                text: qsTr("Restart Software")
                                Layout.fillWidth: true
                                Layout.preferredHeight: 61
                                onClicked: qmlCommand(JSON.stringify({ menuID: "RestartSoftware" }))
                            }
                            Button {
                                text: qsTr("Shutdown")
                                Layout.fillWidth: true
                                Layout.preferredHeight: 61
                                onClicked: qmlCommand(JSON.stringify({ menuID: "ShutdownSoftware" }))
                            }

                            Item { Layout.fillHeight: true } // spacer
                        }
                    }

                    // Empty panel (placeholder)
                    Rectangle {
                        id: rectangle2
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 548

                        color: "#00ffffff"
                        radius: 10
                        opacity: 0.15
                        border.color: "#002e6e86"
                        border.width: 1
                    }
                }
            }
        }
    }
}

/*##^##
Designer {
    D{i:0;formeditorZoom:0.5}
}
##^##*/
