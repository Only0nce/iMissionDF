import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import "ui"

Item {
    id: item1
    width: 600
    height: 400
    property bool darkMode: true
    Theme { id: hmiTheme; darkMode: item1.darkMode }

    property color buttonColor: hmiTheme.accent
    property color buttonColorUnselect: hmiTheme.darkMode ? "#40666666" : "#DDE9E6"

    // ===== ค่า property สำหรับ logic ภายนอก =====
    property real startFreqHz: 88000000      // 88 MHz default
    property real stopFreqHz: 95000000       // 108 MHz default
    property string bandMode: "wide"         // "narrow" หรือ "wide"

    Component.onCompleted: {
        console.log("Component.onCompleted:profilesFromDb")
    }

    Connections {
        target: mainWindows
        ignoreUnknownSignals: true

        function onUpdateCardProfile() {
            profileCardsfns()
        }

        function onProfilesFromDb(list) {
            profilesFromDb(list)
        }
    }

    function profilesFromDb(list){
        profileScan.clear()
        console.log("Total profiles:", (list).length)

        for (var i = 0; i < (list).length; i++) {
            var p = (list)[i]
            profileScan.append({
                index: p.index,
                freq:       p.frequency || p.freq,
                unit:       p.unit || "MHz",
                bw:         p.bw,
                mode:       p.mode,
                low_cut:    p.low_cut,
                high_cut:   p.high_cut,
                time:       p.time
            })
        }
    }

    function profileCardsfns() {
        var size = foundCards.count;
        if (size <= 0) return;
        var profiles = [];
        for (var i = 0; i < size; i++) {
            var item = foundCards.get(i);
            profiles.push({
                index:     item.index,
                frequency: item.freq,
                unit:      item.unit,
                bw:        item.bw,
                startHz:   item.startHz,
                endHz:     item.endHz,
                mode:      item.mode,
                low_cut:  item.low_cut,
                high_cut: item.high_cut
            });
        }
        var msg = { objectName: "profilesCard", profiles: profiles };
        profileWeb(JSON.stringify(msg, null, 2))
        close();
    }

    Label {
        id: title
        width: 320
        height: 35
        text: qsTr("RF Spectrum Analyzer")
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 27
        color: hmiTheme.text
        font.bold: true
        font.pixelSize: 24
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    Rectangle {
        id: freqBox
        width: parent.width - 40
        height: 150
        radius: 12
        color: hmiTheme.input
        border.color: hmiTheme.lineStrong
        border.width: 1
        anchors.top: title.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 39

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 15
            spacing: 10

            RowLayout {
                spacing: 10
                Label {
                    text: "Start Frequency (MHz):"
                    color: hmiTheme.text
                    Layout.alignment: Qt.AlignVCenter
                }
                TextField {
                    id: startField
                    text: (item1.startFreqHz/1e6).toFixed(6)
                    placeholderText: "Enter start freq in MHz"
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    validator: DoubleValidator { bottom: 0; top: 6000; decimals: 6 }
                    Layout.fillWidth: true
                    color: hmiTheme.text
                    placeholderTextColor: hmiTheme.muted
                    selectedTextColor: hmiTheme.darkMode ? "#061514" : "#FFFFFF"
                    selectionColor: hmiTheme.accent
                    background: Rectangle {
                        radius: 8
                        color: hmiTheme.card
                        border.color: hmiTheme.lineStrong
                        border.width: 1
                    }
                    onEditingFinished: item1.startFreqHz = Number(text) * 1e6
                }
            }

            RowLayout {
                spacing: 10
                Label {
                    text: "Stop Frequency (MHz):"
                    color: hmiTheme.text
                    Layout.alignment: Qt.AlignVCenter
                }
                TextField {
                    id: stopField
                    text: (item1.stopFreqHz/1e6).toFixed(6)
                    placeholderText: "Enter stop freq in MHz"
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    validator: DoubleValidator { bottom: 0; top: 6000; decimals: 6 }
                    Layout.fillWidth: true
                    color: hmiTheme.text
                    placeholderTextColor: hmiTheme.muted
                    selectedTextColor: hmiTheme.darkMode ? "#061514" : "#FFFFFF"
                    selectionColor: hmiTheme.accent
                    background: Rectangle {
                        radius: 8
                        color: hmiTheme.card
                        border.color: hmiTheme.lineStrong
                        border.width: 1
                    }
                    onEditingFinished: item1.stopFreqHz = Number(text) * 1e6
                }
            }
        }
    }

    ColumnLayout {
        id: columnLayout
        y: 294
        height: 50
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 12

        RowLayout {
            id: rowLayout
            spacing: 10

            Button {
                id: testButton
                text: "Apply Range"
                font.pixelSize: 18
                property color normalColor: hmiTheme.info
                property color hoverColor: Qt.lighter(hmiTheme.info, 1.12)
                property color pressedColor: Qt.darker(hmiTheme.info, 1.15)
                hoverEnabled: true
                enabled: trigerScan
                onClicked: {
                    trigerScan = false
                    var msg = {
                        "objectName": "Scan",
                        "frequency": { "start": item1.startFreqHz, "stop": item1.stopFreqHz },
                        "modes": ["wide", "narrow"]
                    }
                    sCan(JSON.stringify(msg))
                }
                background: Rectangle {
                    color: !testButton.enabled ? hmiTheme.disabled : (testButton.down ? testButton.pressedColor : (testButton.hovered ? testButton.hoverColor : testButton.normalColor))
                    radius: 18
                    Behavior on color { ColorAnimation { duration: 120 } }
                }
                contentItem: Text {
                    text: testButton.text
                    color: "#FFFFFF"
                    font.pixelSize: testButton.font.pixelSize
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                Layout.preferredHeight: 50
                Layout.fillWidth: true
            }

            Button {
                id: stopButton
                text: "Stop"
                font.pixelSize: 18
                property color normalColor: hmiTheme.danger
                property color hoverColor: Qt.lighter(hmiTheme.danger, 1.08)
                property color pressedColor: Qt.darker(hmiTheme.danger, 1.15)
                hoverEnabled: true
                enabled: !trigerScan
                onClicked: {
                    var msg = { "objectName": "Scan", "action": "stop" }
                    sCan(JSON.stringify(msg))
                    trigerScan = true
                }
                background: Rectangle {
                    color: !stopButton.enabled ? hmiTheme.disabled : (stopButton.down ? stopButton.pressedColor : (stopButton.hovered ? stopButton.hoverColor : stopButton.normalColor))
                    radius: 18
                    Behavior on color { ColorAnimation { duration: 120 } }
                }
                contentItem: Text {
                    text: stopButton.text
                    color: "#FFFFFF"
                    font.pixelSize: stopButton.font.pixelSize
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                Layout.preferredHeight: 50
                Layout.fillWidth: true
            }
        }
    }
}
