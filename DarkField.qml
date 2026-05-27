import QtQuick 2.15
import QtQuick.Controls 2.15

TextField {
    id: root

    property color textColor: "#e6edf3"
    property color accentColor: "#00c896"
    property color borderColor: "#2f4055"
    property color fillColor: "#0d1520"
    property int fieldHeight: 42
    property int fieldRadius: 8
    property int fieldFontSize: 15
    property int fieldPadding: 12
    property bool actionVisible: false
    property string actionText: ""
    property int actionButtonWidth: 42
    property int actionFontSize: 12
    property bool selectAllOnFocus: true

    signal actionClicked()

    color: textColor
    placeholderTextColor: "#697789"
    selectionColor: accentColor
    selectedTextColor: "#061016"

    font.pixelSize: fieldFontSize

    // สำคัญ: ล็อกความสูงเดิม แต่จัด text ให้อยู่กลาง input box
    implicitHeight: fieldHeight
    height: fieldHeight
    verticalAlignment: TextInput.AlignVCenter
    horizontalAlignment: TextInput.AlignLeft
    clip: true

    // สำคัญ: อย่าใช้ fieldPadding เป็น padding บน/ล่าง
    // เพราะ fieldHeight บางช่องแค่ 26 ถ้า top/bottom = 6 text จะลอย/ถูกบีบ
    leftPadding: fieldPadding
    rightPadding: fieldPadding + (actionVisible ? actionButtonWidth : 0)
    topPadding: 0
    bottomPadding: 0


    onActiveFocusChanged: {
        if (activeFocus && selectAllOnFocus) {
            Qt.callLater(function() {
                if (root.activeFocus && root.text.length > 0) {
                    root.selectAll()
                }
            })
        }
    }

    background: Rectangle {
        anchors.fill: parent
        radius: root.fieldRadius
        color: root.fillColor
        border.color: root.activeFocus ? root.accentColor : root.borderColor
        border.width: 1
    }

    Rectangle {
        visible: root.actionVisible
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 1
        width: root.actionButtonWidth
        radius: root.fieldRadius
        color: actionMouseArea.pressed
               ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.22)
               : "transparent"

        Text {
            anchors.fill: parent
            text: root.actionText
            color: root.accentColor
            font.pixelSize: root.actionFontSize
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        MouseArea {
            id: actionMouseArea
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: root.actionClicked()
        }
    }
}
