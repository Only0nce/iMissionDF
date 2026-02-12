import QtQuick 2.0
import QtMultimedia 5.12
Item {
    property string ipAddress: currentIpAddress
    Component.onCompleted: {
        console.log(videoPlayer.source)
    }
    onIpAddressChanged: {
        if ((ipAddress != "") & (ipAddress != "Please Select")){
            if (ipAddress == "192.168.9.18")
                videoPlayer.source = "rtsp://admin:AdminPassword@"+ipAddress+":554/Streaming/Channels/105"
            else if (ipAddress == "192.168.9.102")
                videoPlayer.source = "rtsp://admin:iPro12345@"+ipAddress+":554/mediainput/h264/stream_1"
            else if (ipAddress == "192.168.9.107")
                videoPlayer.source = "rtsp://admin:iPro12345@"+ipAddress+":554/mediainput/h265/stream_1"
        }
        else{
            videoPlayer.stop()
            videoPlayer.source = ""
        }
    }

    MediaPlayer
    {
    id: videoPlayer
        source: ipAddress != "Please Select" & ipAddress != "" ? "rtsp://admin:AdminPassword@"+ipAddress+":554/Streaming/Channels/105" : ""
        muted: true
        autoPlay: false
        onSourceChanged: {
            console.log(source)
            if (source != "")
            {
                play()
                autoPlay = true
            }
            else
            {
                stop()
            }
        }
    }

    VideoOutput {
        id: camera1
        anchors.fill: parent
        anchors.horizontalCenter: parent.horizontalCenter
        source: videoPlayer
    }
}
