import QtQuick 2.12
import QtQuick.Controls 2.12

AppButton {
    id: root
    property bool selected: false
    property color accentColor: "#00c9a7"
    property color panelColor: "#132235"
    property color selectedTextColor: "#001412"
    property color normalTextColor: "#e9f0f7"

    buttonHeight: 42
    buttonRadius: 9
    buttonFontSize: 14
    textColor: selected ? selectedTextColor : normalTextColor
    baseColor: selected ? accentColor : panelColor
}
