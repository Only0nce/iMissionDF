import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import QtPositioning 5.5
import QtLocation 5.6
import QtQuick.Controls.Material 2.15
import "."

Rectangle {
    id: root
    width: 1920
    height: 1080
    color: "#121212"

    Material.theme: Material.Dark
    Material.accent: Material.Teal

    Plugin {
        id: mapPlugin
        name: "osm"

        PluginParameter { name: "osm.mapping.offline.directory"; value: "/home/orinnx/map_tiles1/zurich_tiles" }
        PluginParameter { name: "osm.mapping.providersrepository.disabled"; value: "true" }
        PluginParameter { name: "osm.mapping.offline.mode"; value: "1" }
        PluginParameter { name: "osm.mapping.highdpi_tiles"; value: "true" }
        PluginParameter { name: "osm.mapping.cache.directory"; value: "/home/orinnx/map_cache" }
    }

    Map {
        id: map
        anchors.fill: parent
        plugin: mapPlugin
        center: QtPositioning.coordinate(13.75398, 100.50144)
        zoomLevel: 14
        gesture.enabled: true

        MapQuickItem {
            id: zurichMarker
            coordinate: QtPositioning.coordinate(13.75398, 100.50144)
            anchorPoint.x: markerImage.width/2
            anchorPoint.y: markerImage.height

            sourceItem: Column {
                spacing: 2

                Image {
                    id: markerImage
                    source: "qrc:/images/marker.png"
                    width: 32
                    height: 32
                }

                Rectangle {
                    width: label.width + 8
                    height: label.height + 4
                    radius: 4
                    color: "#80000000"
                    border.color: Material.accent
                    border.width: 1

                    Text {
                        id: label
                        anchors.centerIn: parent
                        text: "Bangkok"
                        color: "white"
                        font.pixelSize: 14
                        font.bold: true
                    }
                }
            }
        }

        Column {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 20
            spacing: 10

            RoundButton {
                text: "+"
                width: 40
                height: 40
                onClicked: map.zoomLevel = Math.min(map.zoomLevel + 1, 20)
            }

            RoundButton {
                text: "-"
                width: 40
                height: 40
                onClicked: map.zoomLevel = Math.max(map.zoomLevel - 1, 0)
            }
        }
    }

    Rectangle {
        id: navBar
        width: parent.width
        height: 60
        color: "#000000"
        z: 1
        anchors.top: parent.top

        RowLayout {
            anchors.fill: parent
            spacing: 20
            anchors.leftMargin: 20
            anchors.rightMargin: 20

            RoundButton {
                id: settingsButton
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                Layout.alignment: Qt.AlignVCenter
                radius: 24
                padding: 6

                background: Rectangle {
                    color: settingsButton.pressed ? "#0E6B56" :
                           settingsButton.hovered ? "#1AA987" : "#169976"
                    border.color: "#ffffff"
                    border.width: settingsButton.hovered ? 2 : 1
                    radius: 24
                    anchors.fill: parent
                    Behavior on color {
                        ColorAnimation { duration: 200 }
                    }
                }

                Image {
                    source: "qrc:/images/gear.png"
                    anchors.fill: parent
                    anchors.margins: 10
                    fillMode: Image.PreserveAspectFit
                }

                ToolTip.visible: hovered
                ToolTip.text: qsTr("Settings")

                onClicked: {
                    settingsDrawer.open()
                }
            }

            Label {
                text: "Offline Map Viewer"
                font.pixelSize: 20
                font.bold: true
                color: "#169976"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                id: locationLabel
                text: "47.3769°N, 8.5417°E"
                font.pixelSize: 14
                color: "#169976"
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }

    Drawer {
        id: settingsDrawer
        width: 530
        height: window.height
        edge: Qt.LeftEdge
        modal: true
        interactive: true
        visible: false

        background: Rectangle {
            color: "#202020"
        }

        Item {
            anchors.fill: parent
            anchors.margins: 20

            ColumnLayout {
                anchors.fill: parent
                spacing: 20

                Label {
                    text: "Mode"
                    font.pixelSize: 20
                    font.bold: true
                    color: "#ffffff"
                }
                ComboBox {
                    id: pageSelector
                    Layout.preferredWidth: 300

                    model: ListModel {
                        ListElement { title: "MAP VISUALIZATION"; source: "qrc:QMLMap.qml" }
                        ListElement { title: "SPECTRUM"; source: "qrc:/spectrum.qml" }
                        ListElement { title: "DOA ESTIMATION"; source: "qrc:/estimation.qml" }
                    }

                    textRole: "title"

                    onCurrentIndexChanged: {                        loader.source = model.get(currentIndex).source
                    }
                }

                Label {
                    text: "DAQ Subsystem Status"
                    font.pixelSize: 20
                    font.bold: true
                    color: "#ffffff"
                }
                GridLayout {
                    columns: 2
                    Layout.fillWidth: true
                    rowSpacing: 10
                    columnSpacing: 100

                    Label {
                        text: "Update Rate:"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }
                    Label {
                        text: "40 ms"
                        font.pixelSize: 16
                        color: "#00FFAA"
                    }

                    Label {
                        text: "Latency:"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }
                    Label {
                        text: "564 ms"
                        font.pixelSize: 16
                        color: "#00FFAA"
                    }

                    Label {
                        text: "Frame Index:"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }
                    Label {
                        text: "7872"
                        font.pixelSize: 16
                        color: "#00FFAA"
                    }
                    Label {
                        text: "Frame Type:"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }
                    Label {
                        text: "Data"
                        font.pixelSize: 16
                        color: "#00FFAA"
                    }
                    Label {
                        text: "Frame Sync:"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }
                    Label {
                        text: "Ok"
                        font.pixelSize: 16
                        color: "#00FFAA"
                    }
                    Label {
                        text: "Power level:"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }
                    Label {
                        text: "Ok"
                        font.pixelSize: 16
                        color: "#00FFAA"
                    }
                    Label {
                        text: "Connection Status:"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }
                    Label {
                        text: "Connected"
                        font.pixelSize: 16
                        color: "#00FFAA"
                    }
                    Label {
                        text: "Sample Delay Sync:"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }
                    Label {
                        text: "Ok"
                        font.pixelSize: 16
                        color: "#00FFAA"
                    }
                    Label {
                        text: "IQ Sync:"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }
                    Label {
                        text: "Ok"
                        font.pixelSize: 16
                        color: "#00FFAA"
                    }
                    Label {
                        text: "Noise Source State:"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }
                    Label {
                        text: "Disabled"
                        font.pixelSize: 16
                        color: "#00FFAA"
                    }
                    Label {
                        text: "Center Frequecy [MHz]:"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }
                    Label {
                        text: "144.0"
                        font.pixelSize: 16
                        color: "#00FFAA"
                    }
                    Label {
                        text: "Sampilng Frequecy [MHz]:"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }
                    Label {
                        text: "2.4"
                        font.pixelSize: 16
                        color: "#00FFAA"
                    }
                    Label {
                        text: "DSP Decimated BW [MHz]:"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }
                    Label {
                        text: "2.400"
                        font.pixelSize: 16
                        color: "#00FFAA"
                    }
                    Label {
                        text: "VFO Range [MHz]:"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }
                    Label {
                        text: "142.800 - 145.200"
                        font.pixelSize: 16
                        color: "#00FFAA"
                    }
                    Label {
                        text: "Data Block Length [ms]:"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }
                    Label {
                        text: "436"
                        font.pixelSize: 16
                        color: "#00FFAA"
                    }
                    Label {
                        text: "RF Gains [bB]:"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }
                    Label {
                        text: "0.9,0.9,0.9,0.9,0.9"
                        font.pixelSize: 16
                        color: "#00FFAA"
                    }
                    Label {
                        text: "VFO-0 Power [dB]:"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }
                    Label {
                        text: "-60.0"
                        font.pixelSize: 16
                        color: "#00FFAA"
                    }
                }

                Label {
                    text: "RF Receiver Configuration"
                    font.pixelSize: 20
                    font.bold: true
                    color: "#ffffff"
                }

                GridLayout {
                    columns: 2
                    Layout.fillWidth: true
                    rowSpacing: 10
                    columnSpacing: 100

                    Label {
                        text: "Signal Strength:"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }

                    TextField {
                        id: signalField
                        text: "144"
                        onTextChanged: signalStrength = text
                        font.pixelSize: 16
                        color: "#00FFAA"
                        Layout.preferredWidth: 150
                        Layout.preferredHeight: 32

                        background: Rectangle {
                            color: "#2a2a2a"
                            radius: 10
                            border.color: "#00FFAA"
                            border.width: 1
                        }
                    }

                    Label {
                        text: "Receiver Gain:"
                        font.pixelSize: 16
                        color: "#ffffff"
                    }

                    TextField {
                        id: gainField
                        text: "0.9 dB"
                        onTextChanged: receiverGain = text
                        font.pixelSize: 16
                        color: "#00FFAA"
                        Layout.preferredWidth: signalField.Layout.preferredWidth
                        Layout.preferredHeight: 32

                        background: Rectangle {
                            color: "#2a2a2a"
                            radius: 10
                            border.color: "#00FFAA"
                            border.width: 1
                        }
                    }

                    Button {
                        text: "Update Receiver Parameters"
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        height: 36
                        background: Rectangle {
                            color: "#169976"
                            radius: 6
                        }
                        font.pixelSize: 16
                    }

                    RowLayout {
                        Layout.columnSpan: 2
                        spacing: 10

                        Label {
                            text: "Basic DAQ Configuration"
                            font.pixelSize: 16
                            color: "#ffffff"
                            verticalAlignment: Text.AlignVCenter
                            Layout.alignment: Qt.AlignVCenter
                        }

                        CheckBox {
                            id: autoUpdateCheck
                            Layout.alignment: Qt.AlignVCenter
                            indicator: Rectangle {
                                implicitWidth: 25
                                implicitHeight: 25
                                radius: 3
                                border.color: "#00FFAA"
                                border.width: 1
                                color: autoUpdateCheck.checked ? "#00FFAA" : "transparent"
                            }
                        }
                    }

                }


                Item {
                    Layout.fillHeight: true
                }

                Loader {
                    id: loader
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    source: pageSelector.model.get(0).source  // โหลดหน้าแรกไว้ก่อน
                }
            }
        }
    }
}
