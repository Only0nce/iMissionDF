// popuppanels/SettingParameter.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.12

Item {
    id: settingparameter
    anchors.fill: parent
    property var krakenmapval: null
    // property var popuppanel: null

    /* ====== Palette / Metrics ====== */
    property color colBg:        "#0f1115"
    property color colCard:      "#1a1e24"
    property color colCardHi:    "#202633"
    property color colBorder:    "#263041"
    property color colAccent:    "#34d399"     // teal green
    property color colAccentDim: "#2aa57a"
    property color colText:      "#e5e7eb"
    property color colSubtext:   "#a3a9b3"
    property int   rad: 12
    property int   pad: 10
    property int   rowH: 40

    Rectangle { anchors.fill: parent; anchors.leftMargin: 0; anchors.rightMargin: 0; anchors.topMargin: -1; anchors.bottomMargin: 1; color: colBg }

    Rectangle {
        id: panelBg
        anchors.fill: parent
        anchors.margins: 6
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        anchors.topMargin: 4
        anchors.bottomMargin: 46
        color: colCard
        radius: rad
        border.color: colBorder

        ColumnLayout {
            id: mainCol
            anchors.fill: parent
            anchors.margins: 24
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.topMargin: 12
            anchors.bottomMargin: 12
            spacing: 18

            // ===== Title =====
            Label {
                color: "#ffffff"
                text: "Device Parameters"
                font.pixelSize: 18
                font.bold: true
                Layout.alignment: Qt.AlignLeft
            }
            // ===== Scrollable Form =====
            ScrollView {
                id: scrollView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                background: Rectangle {
                    radius: rad
                    color: colCardHi
                    border.color: colBorder
                }
                GridLayout {
                    id: grid
                    height: 712
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.leftMargin: 100
                    anchors.topMargin: 10
                    columns: 2
                    rowSpacing: 12
                    columnSpacing: 18

                    // --- Device Name ---
                    Label {
                        text: "Device Name"
                        color: colSubtext
                        font.pixelSize: 14
                        Layout.alignment: Qt.AlignRight
                    }
                    TextField {
                        id: nameField
                        text: "KrakenNode_01"
                        placeholderText: "Enter device name"

                        Layout.fillWidth: true
                        Layout.preferredHeight: 34

                        color: colText
                        font.pixelSize: 15

                        leftPadding: 10
                        rightPadding: 10
                        topPadding: 4
                        bottomPadding: 4
                        background: Rectangle {
                            radius: 8
                            color: "#12161d"
                            border.color: colBorder
                            border.width: 1
                        }
                    }

                    // --- Center Frequency ---
                    Label { text: "Center Frequency (MHz)"; color: colSubtext; Layout.alignment: Qt.AlignRight }
                    SpinBox {
                        id: freqSpin
                        from: 70000; to: 6000000; value: 144500; stepSize: 50; editable: true
                        Layout.preferredWidth: 180
                        textFromValue: function(v) { return (v / 1000).toFixed(3) }
                        valueFromText: function(t) { var mhz = parseFloat(t); return isNaN(mhz)?0:Math.round(mhz*1000) }
                    }

                    // --- Receiver Gain ---
                    Label { text: "Receiver Gain (dB)"; color: colSubtext; Layout.alignment: Qt.AlignRight }
                    RowLayout {
                        Layout.fillWidth: true
                        Slider { id: gainSlider; from: 0; to: 50; stepSize: 1; value: 20; Layout.fillWidth: true }
                        Label { text: gainSlider.value.toFixed(0); color: colText; Layout.preferredWidth: 36; horizontalAlignment: Text.AlignHCenter }
                    }

                    // --- Mode ---
                    Label { text: "Mode"; color: colSubtext; Layout.alignment: Qt.AlignRight }
                    ComboBox { id: modeCombo; model: ["FM","AM","USB","LSB"]; currentIndex: 0; Layout.preferredWidth: 140 }

                    // --- AGC ---
                    Label { text: "AGC"; color: colSubtext; Layout.alignment: Qt.AlignRight }
                    CheckBox { id: agcCheck; checked: true }

                    // --- Sample Rate ---
                    Label { text: "Sample Rate (MSps)"; color: colSubtext; Layout.alignment: Qt.AlignRight }
                    SpinBox {
                        id: sampleRateSpin
                        from: 1e6; to: 20e6; value: 2e6; stepSize: 2.5e5; editable: true
                        Layout.preferredWidth: 180
                        textFromValue: v => (v / 1e6).toFixed(2)
                        valueFromText: t => Math.round(parseFloat(t) * 1e6)
                    }

                    // --- Bandwidth ---
                    Label { text: "Bandwidth (kHz)"; color: colSubtext; Layout.alignment: Qt.AlignRight }
                    SpinBox {
                        id: bwSpin
                        from: 6000; to: 1000000; value: 500000; stepSize: 5000; editable: true
                        Layout.preferredWidth: 180
                        textFromValue: v => (v / 1000).toFixed(0)
                        valueFromText: t => Math.round(parseFloat(t) * 1000)
                    }

                    // --- Squelch Threshold ---
                    Label { text: "Squelch (dBFS)"; color: colSubtext; Layout.alignment: Qt.AlignRight }
                    SpinBox { id: squelchSpin; from: -90; to: 0; value: -70; stepSize: 1; editable: true }

                    // --- LO Offset ---
                    Label { text: "LO Offset (kHz)"; color: colSubtext; Layout.alignment: Qt.AlignRight }
                    SpinBox { id: loSpin; from: -500; to: 500; value: 0; stepSize: 1 }

                    // --- Antenna Select ---
                    Label { text: "Antenna"; color: colSubtext; Layout.alignment: Qt.AlignRight }
                    ComboBox { id: antennaCombo; model: ["AUTO", "ANT 1", "ANT 2", "ANT 3"]; Layout.preferredWidth: 140 }

                    // --- Filter Type ---
                    Label { text: "Filter Type"; color: colSubtext; Layout.alignment: Qt.AlignRight }
                    ComboBox { id: filterCombo; model: ["None", "Lowpass", "Highpass", "Bandpass"]; Layout.preferredWidth: 140 }

                    // --- FFT Size ---
                    Label { text: "FFT Size"; color: colSubtext; Layout.alignment: Qt.AlignRight }
                    ComboBox { id: fftSizeCombo; model: ["512", "1024", "2048", "4096"]; currentIndex: 2 }

                    // --- Update Rate ---
                    Label { text: "Update Rate (Hz)"; color: colSubtext; Layout.alignment: Qt.AlignRight }
                    SpinBox { id: updateRateSpin; from: 1; to: 50; value: 5; stepSize: 1 }

                    // --- Noise Reduction ---
                    Label { text: "Noise Reduction"; color: colSubtext; Layout.alignment: Qt.AlignRight }
                    Slider { id: noiseSlider; from: 0; to: 1; value: 0.2 }

                    // --- Volume ---
                    Label { text: "Audio Volume"; color: colSubtext; Layout.alignment: Qt.AlignRight }
                    Slider { id: volSlider; from: 0; to: 100; value: 50 }

                    // --- Recording ---
                    Label { text: "Data Recording"; color: colSubtext; Layout.alignment: Qt.AlignRight }
                    CheckBox { id: recCheck; checked: false }

                    // --- Time Sync ---
                    Label { text: "Time Sync Source"; color: colSubtext; Layout.alignment: Qt.AlignRight }
                    ComboBox { id: timeCombo; model: ["Internal", "GPS", "NTP"]; Layout.preferredWidth: 140 }

                    // --- DOA Algorithm ---
                    Label { text: "DOA Algorithm"; color: colSubtext; Layout.alignment: Qt.AlignRight }
                    ComboBox { id: doaAlgoCombo; model: ["MUSIC", "ESPRIT", "FFT-Peak"]; Layout.preferredWidth: 140 }

                    // --- DOA Smoothing ---
                    Label { text: "DOA Smoothing"; color: colSubtext; Layout.alignment: Qt.AlignRight }
                    Slider { id: smoothSlider; from: 0; to: 1; value: 0.3 }

                    // --- Peak Threshold ---
                    Label { text: "Peak Threshold (dB)"; color: colSubtext; Layout.alignment: Qt.AlignRight }
                    SpinBox { id: peakSpin; from: 0; to: 30; value: 10; stepSize: 1 }

                    Label { text: ""; color: colSubtext; Layout.alignment: Qt.AlignRight }
                }
            }

        }
    }

    // CancelButtonPopupSettingDrawer {
    //     id: cancelBtn
    //     anchors.right: parent.right
    //     anchors.bottom: parent.bottom
    //     anchors.rightMargin: 85
    //     anchors.bottomMargin: 0
    //     onClicked: deleteGroup(selectedGroupIndex)
    //     // onClicked: popuppanel.close()
    // }

    ApplyButtonPopupSettingDrawer {
        id: applyBtn
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 0
        anchors.bottomMargin: 0
        onClicked: {
            console.log("==== Group Summary (by idStr) ====")
            for (var i = 0; i < groupModel.count; i++) {
                var g = groupModel.get(i)
                console.log("ALL Group: ",i, g.name, "count:", g.count, "devices:", g.devices)
            }
            popuppanel.close()
        }
    }
}
