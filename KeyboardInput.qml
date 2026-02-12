import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12
import QtQuick.VirtualKeyboard 2.12
import QtQuick.VirtualKeyboard 2.4


Item {
    id: _item
    property var keyboardLayout: inputPanel.keyboard.layout
    property var keyboardLayout2: inputPanel.keyboard.layout
    property alias inputPanel: inputPanel
    onKeyboardLayoutChanged: {
        if(keyboardLayout !== ""){
            console.log("keyboardLayout1",keyboardLayout)
            var ChangeLanguageKey= findChildByProperty(inputPanel.keyboard, "objectName", "changeLanguageKey", null)
            if(ChangeLanguageKey){
                ChangeLanguageKey.visible=false
            }
        }
    }
    InputPanel {
        id: inputPanel
        y: rotation==270 ? 525 : window.height+height
        x: rotation==270 ? 150 :(window.width/2) - (inputPanel.width/2)
        z: 99
        width: rotation==270 ? 750 : 1024
        visible: true
        rotation: screenrotation

        onVisibleChanged: {
            keyboardLayout2 = inputPanel.keyboard.layout
            if(keyboardLayout2 !== ""){
                var ChangeLanguageKey2= findChildByProperty(inputPanel.keyboard, "objectName", "changeLanguageKey", null)
                if(ChangeLanguageKey2){
                    ChangeLanguageKey2.visible=false
                }
            }
        }
        states: State {
            name: "visible"
            when: inputPanel.active
            PropertyChanges {
                target: inputPanel
                x: screenrotation == 270 ? -75 : (window.width/2) - (inputPanel.width/2)
                y: screenrotation == 270 ? 525 : window.height - inputPanel.height
            }
        }
        transitions: Transition {
            from: ""
            to: "visible"
            reversible: true
            ParallelAnimation {
                NumberAnimation {
                    properties: screenrotation == 270 ? "x" : "y"
                    duration: 250
                    easing.type: Easing.InOutQuad
                }
            }
        }
        onWidthChanged: {
                    console.log("Keyboard width:", width,x,y)
                }
    }
}
