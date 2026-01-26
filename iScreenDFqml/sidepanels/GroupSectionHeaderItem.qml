// GroupSectionHeaderItem.qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: header
    width: ListView.view ? ListView.view.width : parent.width
    height: 34
    radius: 8

    // ===== Inputs =====
    property var    view
    property string selectedGroup
    property var    countInGroup
    property var    firstIndexOfGroup
    property string sectionText: section   // มาจาก context ของ section.delegate

    // ===== Signals =====
    signal clickedSection(string groupName)

    color:        (selectedGroup === sectionText) ? "#20633a" : "#132a1e"
    border.color: (selectedGroup === sectionText) ? "#48ff9a" : "#214a36"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        Text {
            text: sectionText
            color: "#e6f7ec"
            font.pixelSize: 14
            font.bold: true
        }
        Text {
            text: "(" + (countInGroup ? countInGroup(sectionText) : 0) + ")"
            color: "#9bd9b3"
            font.pixelSize: 12
        }
        Item { Layout.fillWidth: true }  // << ใช้ได้เมื่ออยู่ใน RowLayout
    }

    MouseArea {
        anchors.fill: parent
        onClicked: header.clickedSection(sectionText)
    }
}
