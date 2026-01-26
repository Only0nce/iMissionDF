import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
Item {
    id: element
    width: 360
    height: 120
    property string fileName: ""
    property string fileLibName: ""
    property string libNameText: ""
    property string percentmatchText: ""
    property string faceDateTimeText: ""
    property string faceDateText: ""
    property string faceTimeText: ""
    property alias buttonImage: buttonImage
    //    property alias buttonImageLib: buttonImageLib


    onFaceDateTimeTextChanged:
    {
        faceDateText = faceDateTimeText.split('T')[0]
        faceTimeText = (faceDateTimeText.split('T')[1]).split('+')[0]
    }
    Rectangle
        {
            id: background
            anchors.fill: parent
            color: "#33000000"
        }

    RowLayout {
        x: -1
        y: -1
        anchors.rightMargin: 1
        anchors.leftMargin: 1
        anchors.bottomMargin: 1
        anchors.topMargin: 1
        anchors.fill: parent


        ToolButton {
            id: buttonImage
            Layout.fillHeight: true
            Layout.preferredWidth: 120
            contentItem: Image {
                id: image
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
                source: fileName
            }
        }

        Item {
            id: element1
            Layout.preferredWidth: 100
            Layout.fillHeight: true

            Rectangle {
                id: rectangle
                x: 105
                width: 90
                height: 90
                radius: 50
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                Layout.preferredHeight: 90
                Layout.preferredWidth: 90
                color: "transparent"

                //this Rectangle is needed to keep the source image's fillMode
                Rectangle {
                    id: imageSource

                    anchors.fill: parent
                    Image {
                        anchors.fill: parent
                        source: fileLibName

                        fillMode: Image.PreserveAspectCrop
                    }
                    visible: false

                    layer.enabled: true
                }

                Rectangle {
                    id: maskLayer
                    anchors.fill: parent
                    radius: parent.width / 2

                    color: "red"

                    border.color: "black"

                    layer.enabled: true
                    layer.samplerName: "maskSource"
                    layer.effect: ShaderEffect {

                        property var colorSource: imageSource
                        fragmentShader: "
                            uniform lowp sampler2D colorSource;
                            uniform lowp sampler2D maskSource;
                            uniform lowp float qt_Opacity;
                            varying highp vec2 qt_TexCoord0;
                            void main() {
                                gl_FragColor =
                                    texture2D(colorSource, qt_TexCoord0)
                                    * texture2D(maskSource, qt_TexCoord0).a
                                    * qt_Opacity;
                            }
                        "
                    }

                }

                // only draw border line
                Rectangle {
                    anchors.fill: parent

                    radius: parent.width / 2

                    border.color: "black"
                    border.width: 2

                    color: "transparent"
                }
            }

            Label {
                id: libName
                y: 7
                color: "#ffffff"
                text: libNameText
                anchors.left: parent.left
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 4
                anchors.horizontalCenter: parent.horizontalCenter
                font.pointSize: 14
            }
        }

        Item {
            id: element2
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                x: 45
                y: 44
                anchors.right: parent.right
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter

                Label {
                    id: percentmatch
                    color: "#ffffff"
                    text: percentmatchText
                    Layout.fillWidth: true
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    font.pointSize: 14
                }

                Label {
                    id: faceDate
                    color: "#ffffff"
                    text: faceDateText
                    Layout.fillWidth: true
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    font.pointSize: 14
                }

                Label {
                    id: faceTime
                    color: "#ffffff"
                    text: faceTimeText
                    Layout.fillWidth: true
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                    font.pointSize: 14
                }
            }
        }

    }


}











/*##^## Designer {
    D{i:1;anchors_height:200;anchors_width:200}D{i:3;anchors_width:120}D{i:7;anchors_width:120}
D{i:6;anchors_width:120;anchors_x:120;anchors_y:0}D{i:5;anchors_x:105;anchors_y:14}
}
 ##^##*/
