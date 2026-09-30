// ===== MonitorDisplay.qml =====
// Phase 6 HMI migration. Backend bindings and command payloads are preserved.
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Controls.Material 2.12
import QtQuick.Layouts 1.12
import "../ui"

Item {
    id: rootmonitorView
    width: parent ? parent.width : 1920
    height: parent ? parent.height : 1080
    clip: true
    readonly property bool compactLayout: height < 850 || width < 1200

    property bool isDarkTheme: Material.theme === Material.Dark
    Theme { id: hmiTheme; darkMode: rootmonitorView.isDarkTheme }

    property real cpuPct: monitorCpuPct
    property string updatedText: monitorTs
    property real memUsedMB: monitorMemUsed
    property real memTotalMB: monitorMemTotal
    property real percentRAMUsed: monitorPercentRAM
    property real storageUsedGB: monitorStorageUsed
    property real storageTotalGB: monitorStorageTotal
    property string hTopUptimeText: monitorUptimeText
    property real hTopLoad1: monitorLoad1
    property real hTopLoad5: monitorLoad5
    property real hTopLoad15: monitorLoad15
    property int hTopTasksTotal: monitorTasksTotal
    property int hTopThreadsTotal: monitorThreadsTotal
    property int hTopTasksRunning: monitorTasksRunning

    readonly property real storagePct: storageTotalGB > 0 ? storageUsedGB * 100.0 / storageTotalGB : 0
    readonly property bool telemetryLive: updatedText !== ""

    Rectangle {
        anchors.fill: parent
        color: hmiTheme.page
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.bottomMargin: 14
        anchors.topMargin: 74
        spacing: rootmonitorView.compactLayout ? 8 : 12

        // Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: rootmonitorView.compactLayout ? 52 : 62
            radius: hmiTheme.radiusLg
            color: hmiTheme.panel
            border.width: 1
            border.color: hmiTheme.line

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                spacing: 12

                ColumnLayout {
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 1
                    Text {
                        text: "SYSTEM MONITOR"
                        color: hmiTheme.text
                        font.pixelSize: 20
                        font.bold: true
                    }
                    Text {
                        text: "Runtime telemetry and operator controls"
                        color: hmiTheme.muted
                        font.pixelSize: 11
                    }
                }

                Item { Layout.fillWidth: true }

                HmiStatusPill {
                    darkMode: rootmonitorView.isDarkTheme
                    text: rootmonitorView.telemetryLive ? "TELEMETRY LIVE" : "WAITING"
                    tone: rootmonitorView.telemetryLive ? "good" : "warn"
                    compact: true
                }

                Text {
                    text: rootmonitorView.updatedText !== "" ? ("Updated  " + rootmonitorView.updatedText) : ""
                    color: hmiTheme.muted
                    font.pixelSize: 11
                }
            }
        }

        // CPU / RAM / Storage
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: rootmonitorView.compactLayout
                                    ? Math.max(210, Math.min(240, rootmonitorView.height * 0.34))
                                    : Math.max(330, Math.min(430, rootmonitorView.height * 0.44))
            spacing: 12

            CPUused {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                darkMode: rootmonitorView.isDarkTheme
                valuePct: rootmonitorView.cpuPct
                updatedText: rootmonitorView.updatedText
            }

            RAMused {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                darkMode: rootmonitorView.isDarkTheme
                usedMB: rootmonitorView.memUsedMB
                totalMB: rootmonitorView.memTotalMB
                valuePct: rootmonitorView.percentRAMUsed
                updatedText: rootmonitorView.updatedText
            }

            StorageUsed {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                darkMode: rootmonitorView.isDarkTheme
                usedGB: rootmonitorView.storageUsedGB
                totalGB: rootmonitorView.storageTotalGB
                valuePct: rootmonitorView.storagePct
                updatedText: rootmonitorView.updatedText
            }
        }

        // System / Actions / Runtime telemetry
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: rootmonitorView.compactLayout ? 220 : 300
            spacing: 12

            UpTimeUsed {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                darkMode: rootmonitorView.isDarkTheme
                uptimeText: rootmonitorView.hTopUptimeText
                load1: rootmonitorView.hTopLoad1
                load5: rootmonitorView.hTopLoad5
                load15: rootmonitorView.hTopLoad15
                tasksTotal: rootmonitorView.hTopTasksTotal
                threadsTotal: rootmonitorView.hTopThreadsTotal
                tasksRunning: rootmonitorView.hTopTasksRunning
                updatedText: rootmonitorView.updatedText
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                radius: hmiTheme.radiusLg
                color: hmiTheme.panel
                border.color: hmiTheme.line
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 12

                    Text {
                        text: "SYSTEM ACTIONS"
                        color: hmiTheme.text
                        font.pixelSize: 18
                        font.bold: true
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "Use these controls only when an operator has intentionally decided to restart or power down the application platform."
                        wrapMode: Text.WordWrap
                        color: hmiTheme.muted
                        font.pixelSize: 12
                    }

                    HmiButton {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 54
                        darkMode: rootmonitorView.isDarkTheme
                        text: qsTr("RESTART SOFTWARE")
                        tone: "warning"
                        fontPixelSize: 14
                        onClicked: qmlCommand(JSON.stringify({ menuID: "RestartSoftware" }))
                    }

                    HmiButton {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 54
                        darkMode: rootmonitorView.isDarkTheme
                        text: qsTr("SHUTDOWN")
                        tone: "danger"
                        fontPixelSize: 14
                        onClicked: qmlCommand(JSON.stringify({ menuID: "ShutdownSoftware" }))
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                radius: hmiTheme.radiusLg
                color: hmiTheme.panel
                border.color: hmiTheme.line
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 10

                    Text {
                        text: "RUNTIME TELEMETRY"
                        color: hmiTheme.text
                        font.pixelSize: 18
                        font.bold: true
                    }

                    Repeater {
                        model: [
                            { key: "CPU", value: Math.round(rootmonitorView.cpuPct) + "%", tone: rootmonitorView.cpuPct >= 85 ? hmiTheme.danger : (rootmonitorView.cpuPct >= 60 ? hmiTheme.warning : hmiTheme.info) },
                            { key: "RAM", value: Number(rootmonitorView.percentRAMUsed).toFixed(1) + "%", tone: rootmonitorView.percentRAMUsed >= 85 ? hmiTheme.danger : (rootmonitorView.percentRAMUsed >= 60 ? hmiTheme.warning : hmiTheme.info) },
                            { key: "STORAGE", value: Number(rootmonitorView.storagePct).toFixed(1) + "%", tone: rootmonitorView.storagePct >= 90 ? hmiTheme.danger : (rootmonitorView.storagePct >= 75 ? hmiTheme.warning : hmiTheme.info) },
                            { key: "TASKS", value: rootmonitorView.hTopTasksTotal + " total / " + rootmonitorView.hTopTasksRunning + " running", tone: hmiTheme.accent },
                            { key: "THREADS", value: String(rootmonitorView.hTopThreadsTotal), tone: hmiTheme.accent }
                        ]

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 43
                            radius: hmiTheme.radiusMd
                            color: hmiTheme.cardAlt
                            border.width: 1
                            border.color: hmiTheme.line

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 10
                                Text {
                                    text: modelData.key
                                    color: hmiTheme.muted
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: modelData.value
                                    color: modelData.tone
                                    font.pixelSize: 13
                                    font.bold: true
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
