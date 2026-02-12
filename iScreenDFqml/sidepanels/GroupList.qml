import QtQuick 2.15
import QtQuick.Controls 2.15

// Item {
//     id: root
//     anchors.fill: parent

//     // รับ model จากภายนอก (JS array)
//     property var model: []
//     // ปรับแต่งได้จากผู้ใช้คอมโพเนนต์
//     property int spacing: 8

//     ListView {
//         id: listView
//         anchors.fill: parent
//         clip: true
//         spacing: root.spacing
//         model: root.model

//         // delegate ใช้ชนิด QML ที่แยกไฟล์ไว้ (GroupCard.qml)
//         delegate: GroupCard {
//             width: listView.width
//             title:        modelData.title
//             devices:      modelData.devices
//             angleError:   modelData.angleError
//             lobeWidth:    modelData.lobeWidth
//             decaySec:     modelData.decaySec
//             beaconSpeed:  modelData.beaconSpeed
//         }
//     }
// }
