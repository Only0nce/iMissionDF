import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: win
    width: 1920
    height: 1080
    visible: true
    title: "DoaViewerMUSIC"

    color: "#0B1220"

    ViewerPage {
        anchors.fill: parent
    }
}
