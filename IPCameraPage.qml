import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
Item {
    id: element
    width: 1200
    height: 400
    property string backgroundImageFilename: ""
    property real facesize: faceImageListModel.count
    property real faceBlacklistsize: faceImageblacklistModel.count
    property real smartEventImageListSize: smartEventImageList.count
    onFacesizeChanged: {
        faceCaptureGridView.currentIndex = facesize-1
    }
    onFaceBlacklistsizeChanged: {
        backlishfaceCaptureGridView.currentIndex = faceBlacklistsize-1
    }
    onSmartEventImageListSizeChanged: {
        smartEventGridView.currentIndex = smartEventImageListSize-1
    }
    Item {
        id: rectangle
        width: 460
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.top: parent.top

        Label {
            id: label
            x: 159
            height: 25
            color: "#ffffff"
            text: qsTr("Live View")
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
            font.pointSize: 16
            anchors.top: parent.top
            anchors.topMargin: 4
        }


        VideoViewer
        {
            id: videoViewer01
            x: 0
            y: 20
            height: 300
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 20
            anchors.right: parent.right
            anchors.left: parent.left
        }


        Label {
            id: label3
            y: -8
            width: 170
            height: 40
            color: "#ffffff"
            text: qsTr("Cammera List")
            verticalAlignment: Text.AlignVCenter
            anchors.left: parent.left
            anchors.leftMargin: 21
            anchors.top: parent.top
            font.pointSize: 16
            anchors.topMargin: 35
            horizontalAlignment: Text.AlignLeft
        }

        ComboBox {
            id: control
            property int modelWidth: width
            x: 197
            width: 240
            height: 40
            anchors.right: parent.right
            anchors.rightMargin: 25
            anchors.top: parent.top
            anchors.topMargin: 35
            textRole: "deviceNameCnannel"
            delegate: ItemDelegate {
                height: control.height
                width: control.width
                text: control.textRole ? (Array.isArray(control.model) ? modelData[control.textRole] : model[control.textRole]) : "modelData"
                font.weight: control.currentIndex === index ? Font.DemiBold : Font.Normal
                font.family: control.font.family
                font.pointSize: control.font.pointSize
                highlighted: control.highlightedIndex === index
                hoverEnabled: control.hoverEnabled
            }

            TextMetrics {
                id: textMetrics
            }

            model: cammeraList
            onCurrentIndexChanged: {
                currentIpAddress = cammeraList.get(control.currentIndex).ipAddress
                console.log(currentIpAddress)
            }
        }
    }


    Item {
        id: elementFaceCapture
        width: 360
        height: 34
        anchors.top: parent.top
        anchors.leftMargin: 470
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        visible: cameramode == modeFaceCompare
        Label {
            id: label1
            x: 159
            height: 25
            color: "#ffffff"
            text: qsTr("Face Capture")
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
            font.pointSize: 16
            anchors.topMargin: 4
        }
        GridView {
            id: faceCaptureGridView
            anchors.bottomMargin: 10
            anchors.topMargin: 35
            anchors.fill: parent
            delegate: FramePicture {
                id: faceFrame
                fileName: "file:///" + applicationDirPath + "/images/faceImage/"+filename+".jpeg"
                buttonImage.onClicked: {
                    backgroundImageFilename = "file:///" + applicationDirPath + "/images/backgroundImage/"+filename+".jpeg"
                    backgroundImage.visible = true
                }
            }
            model: faceImageListModel
            contentWidth: 120
            contentHeight: 120
            cellHeight: 120
            cellWidth: 120
            flow: GridView.FlowLeftToRight
            snapMode: GridView.NoSnap
            flickableDirection: Flickable.VerticalFlick
            clip: true
        }
    }

    Item {
        id: elementFaceBlacklist
        x: 535
        width: 360
        anchors.right: parent.right
        anchors.bottomMargin: 0
        anchors.topMargin: 0
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        visible: cameramode == modeFaceCompare
        Label {
            id: label2
            x: 159
            height: 25
            color: "#ffffff"
            text: qsTr("Real-Time Analysis")
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
            font.pointSize: 16
            anchors.topMargin: 4
        }

        GridView {
            id: backlishfaceCaptureGridView
            anchors.fill: parent
            delegate: FramePictureBlacklish {
                width: backlishfaceCaptureGridView.cellWidth
                height: backlishfaceCaptureGridView.cellHeight
                id: faceFrame1
                fileName: "file:///" + applicationDirPath + "/images/faceImage/"+filename+".jpeg"
                fileLibName: "file:///" + applicationDirPath + "/images/faceLibImage/"+FDLibName+".jpeg"
                libNameText: FDLibName
                faceDateTimeText: faceTime
                percentmatchText: (maxsimilarity*100).toFixed(2)+"%"
                buttonImage.onClicked: {
                    backgroundImageFilename = "file:///" + applicationDirPath + "/images/backgroundImage/"+filename+".jpeg"
                    backgroundImage.visible = true
                }
                //                buttonImageLib.onClicked: {
                //                    backgroundImageFilename = "file:///" + applicationDirPath + "/images/backgroundImage/"+filename+".jpeg"
                //                    backgroundImage.visible = true
                //                }
            }
            model: faceImageblacklistModel
            cellHeight: 120
            contentWidth: 360
            snapMode: GridView.NoSnap
            contentHeight: 120
            cellWidth: 360
            clip: true
            flickableDirection: Flickable.VerticalFlick
            flow: GridView.FlowLeftToRight
            anchors.bottomMargin: 10
            anchors.topMargin: 35
        }
    }
    ToolButton {
        id: backgroundImage
        visible: false
        anchors.rightMargin: 1
        anchors.leftMargin: -79
        anchors.bottomMargin: 1
        anchors.topMargin: 1
        anchors.fill: parent
        z: 98
        contentItem: Image {
            id: image
            fillMode: Image.PreserveAspectFit
            source: backgroundImageFilename
            anchors.fill: parent
        }
        onClicked: {
            backgroundImage.visible = false
            backgroundImageFilename = ""
        }
    }

    Item {
        id: elementSmartEvent
        x: -9
        y: -8
        height: 34
        anchors.right: parent.right
        anchors.leftMargin: 470
        visible: cameramode == modeSmartEvent
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        Label {
            id: label4
            x: 159
            height: 25
            color: "#ffffff"
            text: qsTr("Smart Event")
            horizontalAlignment: Text.AlignHCenter
            anchors.top: parent.top
            anchors.topMargin: 4
            font.pointSize: 16
            anchors.horizontalCenter: parent.horizontalCenter
        }

        GridView {
            id: smartEventGridView
            delegate: FramePicture {
                id: faceFrame2
                fileName: "file:///" + applicationDirPath + "/images/"+filename
                buttonImage.onClicked: {
                    backgroundImageFilename = "file:///" + applicationDirPath + "/images/"+filename
                    backgroundImage.visible = true
                }
            }

            flow: GridView.FlowLeftToRight
            contentWidth: 120
            snapMode: GridView.NoSnap
            anchors.topMargin: 35
            cellWidth: 120
            contentHeight: 120
            cellHeight: 120
            anchors.bottomMargin: 10
            anchors.fill: parent
            model: smartEventImageList
            clip: true
            flickableDirection: Flickable.VerticalFlick
        }
    }
}





















/*##^## Designer {
    D{i:2;anchors_y:30}D{i:3;anchors_height:200;anchors_x:535;anchors_y:17}D{i:4;anchors_x:149;anchors_y:30}
D{i:6;anchors_height:200;anchors_x:535;anchors_y:17}D{i:7;anchors_height:200;anchors_x:535;anchors_y:30}
D{i:5;anchors_y:35}D{i:1;anchors_height:200;anchors_x:759;anchors_y:112}D{i:9;anchors_height:200;anchors_x:535;anchors_y:17}
D{i:11;anchors_y:30}D{i:10;anchors_height:200;anchors_x:535;anchors_y:30}D{i:8;anchors_height:200;anchors_x:535;anchors_y:30}
D{i:13;anchors_height:200;anchors_x:535;anchors_y:30}D{i:14;anchors_y:30}D{i:12;anchors_height:200;anchors_x:535;anchors_y:30}
}
 ##^##*/
