import QtQuick 2.12
import QtQuick.Controls 2.12

AppButton {
    id: root
    property bool selected: false
    property color accentColor: "#00c9a7"
    property color normalColor: "#0d1723"
    property color selectedTextColor: "#001412"
    property color normalTextColor: "#e9f0f7"

    buttonHeight: 42
    buttonRadius: 8
    buttonFontSize: 14
    textColor: selected ? selectedTextColor : normalTextColor
    baseColor: selected ? accentColor : normalColor
}
