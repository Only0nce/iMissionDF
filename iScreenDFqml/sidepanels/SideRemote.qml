import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3

Item {
    id: remote
    anchors.fill: parent
    property var krakenmapval: null

    // Connections {
    //     target: krakenmapval
    //     // function onShowToast(t) { console.log(t) }
    //     // function onGroupCreated(id, name) { /* update UI / model */ }
    //     function onUpdateRateChanged() {
    //         console.log("Sidegpsalt =", krakenmapval.updateRate)
    //     }
    //     function onLatencyChanged(){
    //         console.log("SideLatency=", krakenmapval.latency)
    //     }

    // }
    // ===== ตัวอย่าง Model (แทน MySQL/Backend) =====
    ListModel {
        id: deviceModel
        ListElement { idStr: "1"; name: "beacon_three"; ip: "192.168.1.10"; port: 8000; status: "Online"; rssi: 4 }
        ListElement { idStr: "2"; name: "beacon_two";   ip: "192.168.1.11"; port: 8001; status: "Online"; rssi: 3 }
        ListElement { idStr: "3"; name: "beacon_one";   ip: "192.168.1.12"; port: 8002; status: "Offline"; rssi: 2 }
    }
    // ===== ฟังก์ชันกิน JSON จาก C++ แล้วเติมลง ListModel =====
        function applyDeviceList(json) {
            try {
                var obj = (typeof json === "string") ? JSON.parse(json) : json
                if (!obj || obj.objectName !== "DeviceList" || !obj.records) {
                    console.warn("[SideRemote] invalid payload:", json)
                    return
                }

                deviceModel.clear()
                for (var i = 0; i < obj.records.length; ++i) {
                    var r = obj.records[i]
                    deviceModel.append({
                        idStr:  String(r.id || (i+1)),
                        name:   String(r.name || ""),
                        ip:     String(r.ip || ""),
                        port:   Number(r.port || 0),
                        status: String(r.status || "Offline"),
                        rssi:   Number(r.rssi || 0)
                    })
                }
            } catch (e) {
                console.warn("[SideRemote] applyDeviceList error:", e, json)
            }
        }

        Connections {
            target: krakenmapval
            enabled: krakenmapval !== null
            function onSetremoteDeviceListJson(json) { applyDeviceList(json) }
        }

        Component.onCompleted: {
            Qt.callLater(function() {
                if (krakenmapval) {
                    krakenmapval.getdatabaseToSideSettingDrawer("SideRemote")
                } else {
                    console.warn("[SideRemote] krakenmapval/getDeviceList not available")
                }
            })
        }

        // ===== UI =====
        Column {
            id: col
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            Row {
                id: headerRow
                height: 36
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 10

                Label {
                    text: "Remote SDRs"
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#e8f5ec"
                    font.pixelSize: 18
                    font.bold: true
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    id: addBtn
                    width: 50; height: 35
                    radius: height/2
                    anchors.right: parent.right
                    anchors.rightMargin: 30
                    color: "#25303b"
                    Image {
                        id: addIcon
                        anchors.centerIn: parent
                        source: "qrc:/iScreenDFqml/images/gearicon.png"  /*"qrc:/images/addicon.png"*/
                        width: 37; height: 37
                        fillMode: Image.PreserveAspectFit
                    }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: addBtn.color = "#324152"
                        onExited:  addBtn.color = "#25303b"
                        onClicked: {
                            // ตัวอย่าง: เพิ่มแถวใหม่ใน UI (ไม่ยุ่ง DB)
                        //     deviceModel.append({
                        //         idStr: String(deviceModel.count + 1),
                        //         name: "device_" + (deviceModel.count + 1),
                        //         ip: "192.168.1." + (10 + deviceModel.count + 1),
                        //         port: 8000 + deviceModel.count + 1,
                        //         status: "Online",
                        //         rssi: (deviceModel.count % 5)
                        //     })
                        if (krakenmapval) krakenmapval.openPopupSetting("Add Device")
                        }
                    }
                }
            }

            Rectangle {
                id: panel
                radius: 10
                color: "#00111212"
                border.color: "#00111212"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: headerRow.bottom
                anchors.bottom: parent.bottom
                anchors.topMargin: 8

                layer.enabled: true
                layer.smooth: true
                layer.mipmap: true

                ListView {
                    id: listView
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8
                    clip: true
                    model: deviceModel
                    focus: true
                    currentIndex: -1

                    delegate: RemoteSdrItem {
                        deviceName: model.name
                        deviceIp: model.ip
                        devicePort: model.port
                        deviceStatus: model.status
                        deviceRssi: model.rssi
                        rowIndex: index
                        isCurrent: ListView.isCurrentItem
                        onClicked: {
                            listView.currentIndex = index
                            console.log("Focus row:", deviceName , deviceIp, devicePort, deviceStatus)
                            // ตัวอย่าง: krakenmapval.connectTo(deviceIp, devicePort) ถ้ามี
                        }
                    }
                }
            }
        }
    }
