import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    property bool darkMode: true
    property string currentLanguage: (typeof translationManager !== "undefined" && translationManager)
                                     ? translationManager.language : "en"
    // R1.7.3B: keep the toggle label ASCII so users can switch to Thai even
    // before Thai fonts are installed or selected by Qt.
    property string thaiDisplayLabel: "TH"

    width: 92
    height: 36

    Theme { id: theme; darkMode: root.darkMode }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: theme.cardAlt
        border.color: theme.lineStrong
        border.width: 1
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 3
        spacing: 3

        Repeater {
            model: [
                { code: "en", label: "EN" },
                { code: "th", label: root.thaiDisplayLabel }
            ]

            delegate: Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: height / 2
                color: root.currentLanguage === modelData.code ? theme.accent : "transparent"
                border.width: root.currentLanguage === modelData.code ? 0 : 1
                border.color: root.currentLanguage === modelData.code ? "transparent" : theme.line

                Text {
                    font.family: (typeof uiFontFamily !== "undefined" && uiFontFamily !== "" ? uiFontFamily : "Noto Sans Thai")
                    anchors.centerIn: parent
                    text: modelData.label
                    color: root.currentLanguage === modelData.code ? "#FFFFFF" : theme.text
                    font.pixelSize: 12
                    font.bold: true
                    renderType: Text.NativeRendering
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof translationManager !== "undefined" && translationManager)
                            translationManager.setLanguage(modelData.code)
                    }
                }
            }
        }
    }

    Connections {
        target: (typeof translationManager !== "undefined") ? translationManager : null
        function onLanguageChanged() {
            root.currentLanguage = translationManager.language
        }
    }
}
