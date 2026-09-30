import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../../ui"

Item {
    id: settingparameter
    anchors.fill: parent

    property var krakenmapval: null
    property bool darkMode: true
    Theme { id: hmiTheme; darkMode: settingparameter.darkMode }

    property color colBg:        hmiTheme.page
    property color colCard:      hmiTheme.card
    property color colCardHi:    hmiTheme.cardAlt
    property color colBorder:    hmiTheme.line
    property color colBorderHi:  hmiTheme.lineStrong
    property color colAccent:    hmiTheme.accent
    property color colAccentDim: hmiTheme.accentHover
    property color colText:      hmiTheme.text
    property color colSubtext:   hmiTheme.textSecondary
    property color colInput:     hmiTheme.input
    property color colDisabled:  hmiTheme.disabled
    property int   rad: 12

    Rectangle {
        anchors.fill: parent
        color: colBg
    }

    Rectangle {
        id: panelBg
        anchors.fill: parent
        anchors.margins: 4
        anchors.bottomMargin: 52
        radius: rad
        color: colCard
        border.color: colBorder
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 34

                Label {
                    text: "Device Parameters"
                    color: colText
                    font.pixelSize: 20
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    id: closeBtn
                    Layout.preferredWidth: 96
                    Layout.preferredHeight: 34
                    radius: 17
                    color: closeMouse.containsMouse ? colAccentDim : colCardHi
                    border.width: 1
                    border.color: closeMouse.containsMouse ? colAccent : colBorderHi

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 6
                        Text {
                            text: "✕"
                            color: closeMouse.containsMouse ? "#FFFFFF" : colText
                            font.pixelSize: 14
                            font.bold: true
                        }
                        Text {
                            text: "Close"
                            color: closeMouse.containsMouse ? "#FFFFFF" : colText
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }

                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof popuppanel !== "undefined" && popuppanel)
                                popuppanel.close()
                        }
                    }
                }
            }

            Rectangle {
                id: formPanel
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: rad
                color: colCardHi
                border.color: colBorder
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 24

                    ColumnLayout {
                        id: leftColumn
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            Label { text: "Device Name"; color: colSubtext; font.pixelSize: 14; font.bold: true }
                            TextField {
                                id: nameField
                                text: "KrakenNode_01"
                                placeholderText: "Enter device name"
                                Layout.fillWidth: true
                                Layout.preferredHeight: 42
                                color: colText
                                placeholderTextColor: hmiTheme.muted
                                selectedTextColor: "#ffffff"
                                selectionColor: colAccent
                                leftPadding: 12
                                rightPadding: 12
                                background: Rectangle {
                                    radius: 8
                                    color: colInput
                                    border.color: nameField.activeFocus ? colAccent : colBorder
                                    border.width: nameField.activeFocus ? 2 : 1
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            Label { text: "Center Frequency (MHz)"; color: colSubtext; font.pixelSize: 14; font.bold: true }
                            SpinBox {
                                id: freqSpin
                                from: 70000; to: 6000000; value: 144500; stepSize: 50; editable: true
                                Layout.fillWidth: true
                                Layout.preferredHeight: 42
                                textFromValue: function(v) { return (v / 1000).toFixed(3) }
                                valueFromText: function(t) { var mhz = parseFloat(t); return isNaN(mhz) ? freqSpin.value : Math.round(mhz * 1000) }
                                validator: DoubleValidator { bottom: 70.0; top: 6000.0; decimals: 3 }
                                contentItem: TextInput {
                                    text: freqSpin.textFromValue(freqSpin.value, freqSpin.locale)
                                    font: freqSpin.font
                                    color: colText
                                    selectionColor: colAccent
                                    selectedTextColor: "#ffffff"
                                    horizontalAlignment: Qt.AlignHCenter
                                    verticalAlignment: Qt.AlignVCenter
                                    readOnly: !freqSpin.editable
                                    validator: freqSpin.validator
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                    onEditingFinished: freqSpin.value = freqSpin.valueFromText(text, freqSpin.locale)
                                }
                                up.indicator: Rectangle {
                                    x: parent.width - width - 4; y: 4
                                    width: 34; height: parent.height - 8
                                    radius: 8
                                    color: parent.pressed ? colAccentDim : colCard
                                    border.color: colBorderHi
                                    Text { anchors.centerIn: parent; text: "+"; color: colText; font.pixelSize: 18; font.bold: true }
                                }
                                down.indicator: Rectangle {
                                    x: 4; y: 4
                                    width: 34; height: parent.height - 8
                                    radius: 8
                                    color: parent.pressed ? colAccentDim : colCard
                                    border.color: colBorderHi
                                    Text { anchors.centerIn: parent; text: "−"; color: colText; font.pixelSize: 18; font.bold: true }
                                }
                                background: Rectangle { radius: 8; color: colInput; border.color: freqSpin.activeFocus ? colAccent : colBorder; border.width: freqSpin.activeFocus ? 2 : 1 }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            Label { text: "Receiver Gain (dB)"; color: colSubtext; font.pixelSize: 14; font.bold: true }
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 42
                                spacing: 12
                                Slider { id: gainSlider; from: 0; to: 50; stepSize: 1; value: 20; Layout.fillWidth: true }
                                Rectangle {
                                    Layout.preferredWidth: 58
                                    Layout.preferredHeight: 36
                                    radius: 8
                                    color: colInput
                                    border.color: colBorder
                                    Label { anchors.centerIn: parent; text: gainSlider.value.toFixed(0); color: colText; font.pixelSize: 15; font.bold: true }
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            Label { text: "Mode"; color: colSubtext; font.pixelSize: 14; font.bold: true }
                            HmiComboBox { id: modeCombo; darkMode: settingparameter.darkMode; model: ["FM", "AM", "USB", "LSB"]; currentIndex: 0; Layout.fillWidth: true }
                        }

                        Item { Layout.fillHeight: true }
                    }

                    Rectangle {
                        Layout.preferredWidth: 1
                        Layout.fillHeight: true
                        color: colBorder
                    }

                    ColumnLayout {
                        id: rightColumn
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            Label { text: "AGC"; color: colSubtext; font.pixelSize: 14; font.bold: true }
                            Rectangle {
                                id: agcField
                                Layout.fillWidth: true
                                Layout.preferredHeight: 42
                                radius: 8
                                color: colInput
                                border.color: agcMouse.containsMouse ? colAccent : colBorder
                                border.width: agcMouse.containsMouse ? 2 : 1

                                property bool checked: true

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 10

                                    Rectangle {
                                        Layout.preferredWidth: 22
                                        Layout.preferredHeight: 22
                                        radius: 6
                                        color: agcField.checked ? colAccent : "transparent"
                                        border.width: 1
                                        border.color: agcField.checked ? colAccent : colBorderHi

                                        Text {
                                            anchors.centerIn: parent
                                            text: agcField.checked ? "✓" : ""
                                            color: "#FFFFFF"
                                            font.pixelSize: 14
                                            font.bold: true
                                        }
                                    }

                                    Label {
                                        text: "Enabled"
                                        color: colText
                                        font.pixelSize: 14
                                        verticalAlignment: Text.AlignVCenter
                                        Layout.fillWidth: true
                                    }
                                }

                                MouseArea {
                                    id: agcMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: agcField.checked = !agcField.checked
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            Label { text: "Sample Rate (MSps)"; color: colSubtext; font.pixelSize: 14; font.bold: true }
                            SpinBox {
                                id: sampleRateSpin
                                from: 1000000; to: 20000000; value: 2000000; stepSize: 250000; editable: true
                                Layout.fillWidth: true
                                Layout.preferredHeight: 42
                                textFromValue: function(v) { return (v / 1000000).toFixed(2) }
                                valueFromText: function(t) { var v = parseFloat(t); return isNaN(v) ? sampleRateSpin.value : Math.round(v * 1000000) }
                                validator: DoubleValidator { bottom: 1.0; top: 20.0; decimals: 2 }
                                contentItem: TextInput {
                                    text: sampleRateSpin.textFromValue(sampleRateSpin.value, sampleRateSpin.locale)
                                    font: sampleRateSpin.font
                                    color: colText
                                    selectionColor: colAccent
                                    selectedTextColor: "#ffffff"
                                    horizontalAlignment: Qt.AlignHCenter
                                    verticalAlignment: Qt.AlignVCenter
                                    readOnly: !sampleRateSpin.editable
                                    validator: sampleRateSpin.validator
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                    onEditingFinished: sampleRateSpin.value = sampleRateSpin.valueFromText(text, sampleRateSpin.locale)
                                }
                                up.indicator: Rectangle {
                                    x: parent.width - width - 4; y: 4
                                    width: 34; height: parent.height - 8
                                    radius: 8; color: parent.pressed ? colAccentDim : colCard; border.color: colBorderHi
                                    Text { anchors.centerIn: parent; text: "+"; color: colText; font.pixelSize: 18; font.bold: true }
                                }
                                down.indicator: Rectangle {
                                    x: 4; y: 4
                                    width: 34; height: parent.height - 8
                                    radius: 8; color: parent.pressed ? colAccentDim : colCard; border.color: colBorderHi
                                    Text { anchors.centerIn: parent; text: "−"; color: colText; font.pixelSize: 18; font.bold: true }
                                }
                                background: Rectangle { radius: 8; color: colInput; border.color: sampleRateSpin.activeFocus ? colAccent : colBorder; border.width: sampleRateSpin.activeFocus ? 2 : 1 }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            Label { text: "Bandwidth (kHz)"; color: colSubtext; font.pixelSize: 14; font.bold: true }
                            SpinBox {
                                id: bwSpin
                                from: 6000; to: 1000000; value: 500000; stepSize: 5000; editable: true
                                Layout.fillWidth: true
                                Layout.preferredHeight: 42
                                textFromValue: function(v) { return (v / 1000).toFixed(0) }
                                valueFromText: function(t) { var v = parseFloat(t); return isNaN(v) ? bwSpin.value : Math.round(v * 1000) }
                                validator: DoubleValidator { bottom: 6; top: 1000; decimals: 0 }
                                contentItem: TextInput {
                                    text: bwSpin.textFromValue(bwSpin.value, bwSpin.locale)
                                    font: bwSpin.font
                                    color: colText
                                    selectionColor: colAccent
                                    selectedTextColor: "#ffffff"
                                    horizontalAlignment: Qt.AlignHCenter
                                    verticalAlignment: Qt.AlignVCenter
                                    readOnly: !bwSpin.editable
                                    validator: bwSpin.validator
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                    onEditingFinished: bwSpin.value = bwSpin.valueFromText(text, bwSpin.locale)
                                }
                                up.indicator: Rectangle {
                                    x: parent.width - width - 4; y: 4
                                    width: 34; height: parent.height - 8
                                    radius: 8; color: parent.pressed ? colAccentDim : colCard; border.color: colBorderHi
                                    Text { anchors.centerIn: parent; text: "+"; color: colText; font.pixelSize: 18; font.bold: true }
                                }
                                down.indicator: Rectangle {
                                    x: 4; y: 4
                                    width: 34; height: parent.height - 8
                                    radius: 8; color: parent.pressed ? colAccentDim : colCard; border.color: colBorderHi
                                    Text { anchors.centerIn: parent; text: "−"; color: colText; font.pixelSize: 18; font.bold: true }
                                }
                                background: Rectangle { radius: 8; color: colInput; border.color: bwSpin.activeFocus ? colAccent : colBorder; border.width: bwSpin.activeFocus ? 2 : 1 }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            Label { text: "Squelch (dBFS)"; color: colSubtext; font.pixelSize: 14; font.bold: true }
                            SpinBox {
                                id: squelchSpin
                                from: -90; to: 0; value: -70; stepSize: 1; editable: true
                                Layout.fillWidth: true
                                Layout.preferredHeight: 42
                                contentItem: TextInput {
                                    text: String(squelchSpin.value)
                                    font: squelchSpin.font
                                    color: colText
                                    selectionColor: colAccent
                                    selectedTextColor: "#ffffff"
                                    horizontalAlignment: Qt.AlignHCenter
                                    verticalAlignment: Qt.AlignVCenter
                                    readOnly: !squelchSpin.editable
                                    validator: IntValidator { bottom: -90; top: 0 }
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                    onEditingFinished: {
                                        var v = parseInt(text)
                                        if (!isNaN(v)) squelchSpin.value = Math.max(squelchSpin.from, Math.min(squelchSpin.to, v))
                                    }
                                }
                                up.indicator: Rectangle {
                                    x: parent.width - width - 4; y: 4
                                    width: 34; height: parent.height - 8
                                    radius: 8; color: parent.pressed ? colAccentDim : colCard; border.color: colBorderHi
                                    Text { anchors.centerIn: parent; text: "+"; color: colText; font.pixelSize: 18; font.bold: true }
                                }
                                down.indicator: Rectangle {
                                    x: 4; y: 4
                                    width: 34; height: parent.height - 8
                                    radius: 8; color: parent.pressed ? colAccentDim : colCard; border.color: colBorderHi
                                    Text { anchors.centerIn: parent; text: "−"; color: colText; font.pixelSize: 18; font.bold: true }
                                }
                                background: Rectangle { radius: 8; color: colInput; border.color: squelchSpin.activeFocus ? colAccent : colBorder; border.width: squelchSpin.activeFocus ? 2 : 1 }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }
    }

    ApplyButtonPopupSettingDrawer {
        id: applyBtn
        darkMode: settingparameter.darkMode
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 8
        anchors.bottomMargin: 2
        onClicked: {
            if (typeof popuppanel !== "undefined" && popuppanel)
                popuppanel.close()
        }
    }
}
