import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.0
import QtQuick.Controls.Material 2.15

Item {
    id: root
    visible: true
    width: 1205
    height: 400

    GridView {
        anchors.fill: parent
        anchors.margins: 8
        cellWidth: 132
        cellHeight: 90
        model: profiles

        delegate: ToolButton {
            width: 130
            height: 88
            property string profileId: model.profileId
            property bool isPermanent: model.isPermanent === true
            contentItem:Rectangle {
                id: bgRect
                radius: 8
                color: isPermanent ? "#10ccccff" : (model.isNew ? "#10ffc0a0" : "#00eeeeee")
                border.color: "#10888888"

                // Animate new profiles
                // SequentialAnimation on color {
                //     running: model.isNew
                //     loops: 1
                //     ColorAnimation { to: "#aaffc0a0"; duration: 120 }
                //     ColorAnimation { to: "#00eeeeee"; duration: 400 }
                // }

                Column {
                    spacing: 4
                    anchors.centerIn: parent

                    Image {
                        source: isPermanent ? "images/newRadio.png" : "images/radioIcon.png"
                        width: 62
                        height: 48
                        fillMode: Image.PreserveAspectFit
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Label {
                        text: model.name
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }

            onClicked:
            {
                if (isPermanent) {
                    const uuid = mainWindows.generateGUID().replace(/[{}]/g, ""); // ✅ convert to string
                    const msg = {
                        type: "createprofile",
                        params: {
                            id: uuid,
                            name: "Untitled",
                            base: "default"
                        }
                    }
                    console.log("uuid",uuid)
                    // mainWindows.sendmessage(JSON.stringify(msg));
                } else {
                    const msg = {
                        type: "selectprofile",
                        params: {
                            profile: profileId
                        }
                    }
                    mainWindows.sendmessage(JSON.stringify(msg));

                    var dspcontrol = {
                        type: "dspcontrol",
                        action: "start"
                    }
                    if (mainWindows && typeof mainWindows.sendmessage === "function")
                    {
                        mainWindows.sendmessage(JSON.stringify(dspcontrol));
                    } else {
                        console.error("mainWindows or sendmessage() is not available");
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        mainWindows.refreshProfiles();
    }
}
