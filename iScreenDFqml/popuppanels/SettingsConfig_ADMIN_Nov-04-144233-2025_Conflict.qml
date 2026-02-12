// /popuppanels/SettingParameter.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.12

Rectangle {
    id: root
    width: 430
    height: parent.height
    color: "#202020"
    radius: 10
    border.color: "#303030"

    property var Krakenmapval: null
    property var topDrawer: null
    property var daqBox: null
    property var toggleDaqButton: null
    property var loader: null
    property bool savedDaqVisible: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 10

        /* ===== NEXT BUTTON (ไป Config) ===== */
        Button {
            id: nextButton
            Layout.alignment: Qt.AlignRight
            Layout.preferredHeight: 50
            Layout.preferredWidth: 50
            font.pixelSize: 16

            background: Rectangle {
                color: nextButton.pressed ? Qt.darker("#202020", 1.3) : "#1e1e1e"
                radius: 18
            }

            contentItem: Image {
                source: "qrc:/images/right-arrow.png"
                anchors.centerIn: parent
                width: 50
                height: 50
                fillMode: Image.PreserveAspectFit
            }

            onClicked: {
                // ถ้ามี drawer configvfoDrawer ให้เปิด
                // configvfoDrawer.open()
                console.log("Next clicked → ไป config vfo")
            }
        }

        /* ===== MODE SELECTOR ===== */
        Label {
            text: "Select Mode"
            font.pixelSize: 20
            font.bold: true
            color: "#ffffff"
        }

        ComboBox {
            id: pageSelector
            Layout.preferredWidth: 300
            model: ListModel {
                ListElement { title: "MAP VISUALIZATION"; source: "qrc:/pages/QMLMap.qml" }
                ListElement { title: "SPECTRUM"; source: "qrc:/pages/spectrum.qml" }
                ListElement { title: "DOA ESTIMATION"; source: "qrc:/pages/estimation.qml" }
            }
            textRole: "title"

            onCurrentIndexChanged: {
                savedDaqVisible = daqBox ? daqBox.visible : true
                let selected = model.get(currentIndex)

                if (loader) {
                    if (selected.source !== "qrc:/pages/QMLMap.qml") {
                        if (loader.depth > 1) loader.pop()
                        loader.push(selected.source)
                    } else {
                        while (loader.depth > 1) loader.pop()
                    }
                }

                if (daqBox) {
                    if (selected.source === "qrc:/pages/spectrum.qml")
                        daqBox.visible = false
                    else
                        daqBox.visible = savedDaqVisible
                }

                if (toggleDaqButton)
                    toggleDaqButton.visible = (selected.source !== "qrc:/pages/spectrum.qml")

                if (Krakenmapval)
                    Krakenmapval.pageChanged(JSON.stringify({
                        "menuID": selected.title,
                        "source": selected.source,
                        "index": currentIndex
                    }))
            }
        }

        /* ===== RF RECEIVER CONFIG ===== */
        Label {
            text: "RF Receiver Configuration"
            font.pixelSize: 20
            font.bold: true
            color: "#ffffff"
        }

        GridLayout {
            columns: 2
            rowSpacing: 20
            columnSpacing: 30

            Label { text: "Center Frequency:"; font.pixelSize: 16; color: "#ffffff" }
            TextField {
                id: centerFrequencyInput
                Layout.preferredWidth: 150
                Layout.preferredHeight: 32
                font.pixelSize: 16
                color: "#00FFAA"
                placeholderText: "30 - 1200"
                placeholderTextColor: "#888"
                inputMethodHints: Qt.ImhDigitsOnly
                text: Krakenmapval ? Krakenmapval.centerFrequency.toString() : ""
                background: Rectangle {
                    color: "#2a2a2a"
                    radius: 10
                    border.color: "#00FFAA"
                    border.width: 1
                }
                Connections {
                    target: Krakenmapval
                    enabled: Krakenmapval
                    function onCenterFrequencyChanged() {
                        centerFrequencyInput.text = Number(Krakenmapval.centerFrequency).toFixed(6)
                    }
                }
            }

            Label { text: "Receiver Gain:"; font.pixelSize: 16; color: "#ffffff" }
            ComboBox {
                id: gainCombo
                model: ["0", "0.9", "1.4", "2.7", "3.7", "7.7", "8.7", "12.5", "14.4", "15.7",
                        "16.6", "19.7", "20.7", "22.9", "25.4", "28.0", "29.7", "32.8",
                        "33.8", "36.4", "37.2", "38.6", "40.2", "42.1", "43.4", "44.5",
                        "48.0", "49.6"]
                font.pixelSize: 16
                Layout.preferredWidth: 150
                Layout.preferredHeight: 32

                Component.onCompleted: updateGainCombo()
                function updateGainCombo() {
                    if (!Krakenmapval) return
                    let rfGainValues = Krakenmapval.rfGain.split(",")
                    if (rfGainValues.length > 0) {
                        let firstGain = rfGainValues[0].trim()
                        let idx = gainCombo.model.indexOf(firstGain)
                        if (idx !== -1) gainCombo.currentIndex = idx
                    }
                }

                Connections {
                    target: Krakenmapval
                    enabled: Krakenmapval
                    function onRfGainChanged() { gainCombo.updateGainCombo() }
                }

                contentItem: Text {
                    text: gainCombo.currentText + " dB"
                    color: "#00FFAA"
                    font.pixelSize: 16
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 10
                }

                delegate: ItemDelegate {
                    width: gainCombo.width
                    contentItem: Text {
                        text: modelData + " dB"
                        font.pixelSize: 12
                        color: "#222831"
                    }
                }

                background: Rectangle {
                    color: "#2a2a2a"
                    radius: 10
                    border.color: "#00FFAA"
                    border.width: 1
                }
            }

            Button {
                id: updateButton
                text: "Update Receiver Parameters"
                Layout.columnSpan: 2
                Layout.fillWidth: true
                height: 36
                font.pixelSize: 16
                background: Rectangle {
                    id: buttonBg
                    color: updateButton.pressed ? Qt.darker("#169976", 1.3) : "#169976"
                    radius: 6
                    Behavior on color {
                        ColorAnimation { duration: 150; easing.type: Easing.InOutQuad }
                    }
                }
                contentItem: Text {
                    text: updateButton.text
                    anchors.centerIn: parent
                    color: "#212121"
                    font.bold: true
                    font.pixelSize: updateButton.font.pixelSize
                }
                onClicked: {
                    if (Krakenmapval)
                        Krakenmapval.updateReceiverParameters(centerFrequencyInput.text, gainCombo.currentText)
                }
            }
        }

        /* ===== VFO CONFIG ===== */
        Label {
            text: "VFO Configuration"
            font.pixelSize: 20
            font.bold: true
            color: "#ffffff"
        }

        // --- ส่วนนี้ตัดให้สั้นลง ---
        // (ใช้ตาม version เดิมของคุณได้เลย)
        // Copy GridLayout VFO configuration จาก code เดิมของคุณ

        /* ===== COMPASS ===== */
        Label {
            text: "Compass"
            font.pixelSize: 20
            font.bold: true
            color: "#ffffff"
        }

        GridLayout {
            columns: 2
            rowSpacing: 20
            columnSpacing: 30

            Label { text: "Degree:"; font.pixelSize: 16; color: "#ffffff" }
            TextField {
                id: degreeInput
                Layout.preferredWidth: 150
                Layout.preferredHeight: 32
                font.pixelSize: 16
                color: "#00FFAA"
                readOnly: true
                text: Krakenmapval ? Number(Krakenmapval.degree).toFixed(3) : ""
                background: Rectangle {
                    color: "#2a2a2a"
                    radius: 10
                    border.color: "#00FFAA"
                    border.width: 1
                }
                Connections {
                    target: Krakenmapval
                    enabled: Krakenmapval
                    function onDegreeChanged() {
                        degreeInput.text = Number(Krakenmapval.degree).toFixed(3)
                    }
                }
            }

            Label { text: "Status:"; font.pixelSize: 16; color: "#ffffff" }
            TextField {
                id: degreeStatus
                Layout.preferredWidth: 300
                Layout.preferredHeight: 32
                font.pixelSize: 16
                color: "#00FFAA"
                readOnly: true
                text: Krakenmapval ? Krakenmapval.degreeStatus : "-"
                background: Rectangle {
                    color: "#2a2a2a"
                    radius: 10
                    border.color: "#00FFAA"
                    border.width: 1
                }
                Connections {
                    target: Krakenmapval
                    enabled: Krakenmapval
                    function onDegreeStatusChanged() {
                        degreeStatus.text = Krakenmapval.degreeStatus
                    }
                }
            }

            Button {
                id: calibrationButton
                text: "Calibration"
                Layout.columnSpan: 2
                Layout.fillWidth: true
                height: 36
                font.pixelSize: 16
                background: Rectangle {
                    radius: 6
                    color: calibrationButton.pressed ? Qt.darker("#169976", 1.4) : "#169976"
                    Behavior on color {
                        ColorAnimation { duration: 150; easing.type: Easing.InOutQuad }
                    }
                }
                contentItem: Text {
                    text: calibrationButton.text
                    anchors.centerIn: parent
                    color: "#212121"
                    font.bold: true
                    font.pixelSize: calibrationButton.font.pixelSize
                }
                onClicked: {
                    if (Krakenmapval) Krakenmapval.Calibration("Calibration")
                }
            }
        }

        /* ===== FLOATING BUTTON ===== */
        Item {
            id: floatingButtonContainer
            width: 60
            height: 60
            Layout.alignment: Qt.AlignLeft | Qt.AlignBottom
            Rectangle {
                width: 60
                height: 60
                radius: 30
                color: "#C7C8CC"
                anchors.centerIn: parent
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (Krakenmapval) Krakenmapval.getsettingDisply("settingDisplytrue")
                        if (topDrawer) topDrawer.close()
                        root.visible = false
                    }
                }
                Image {
                    source: "qrc:/images/sun.png"
                    width: 40
                    height: 40
                    anchors.centerIn: parent
                }
            }
        }
    }
}
