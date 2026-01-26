import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12
import QtQuick.VirtualKeyboard 2.12
import QtQuick.VirtualKeyboard 2.4

Item {
    id: _item

    // keep your helpers
    property var keyboardLayout: inputPanel.keyboard.layout
    property var keyboardLayout2: inputPanel.keyboard.layout
    property alias inputPanel: inputPanel

    InputPanel {
        id: inputPanel
        z: 99
        rotation: screenrotation
        width: rotation == 270 ? 750 : 1024

        // ► IMPORTANT: do NOT keep it always-visible
        visible: Qt.inputMethod.visible

        // start offscreen
        x: rotation == 270 ? 150 : (window.width - width) / 2
        y: rotation == 270 ? 525 : window.height + height

        // animate in/out when visible changes
        Behavior on y { NumberAnimation { duration: 250; easing.type: Easing.InOutQuad } }
        Behavior on x { NumberAnimation { duration: 250; easing.type: Easing.InOutQuad } }

        // when it becomes visible, snap to the on-screen target
        onVisibleChanged: {
            keyboardLayout2 = inputPanel.keyboard.layout
            if (visible) {
                x = screenrotation == 270 ? -75 : (window.width - inputPanel.width) / 2
                y = screenrotation == 270 ? 525 : window.height - inputPanel.height
            } else {
                // slide offscreen
                x = rotation == 270 ? 150 : (window.width - width) / 2
                y = rotation == 270 ? 525 : window.height + height
            }

            if (keyboardLayout2 !== "") {
                var ChangeLanguageKey2 = findChildByProperty(inputPanel.keyboard, "objectName", "changeLanguageKey", null)
                if (ChangeLanguageKey2)
                    ChangeLanguageKey2.visible = false
            }
        }
    }

    onKeyboardLayoutChanged: {
        if (keyboardLayout !== "") {
            var ChangeLanguageKey = findChildByProperty(inputPanel.keyboard, "objectName", "changeLanguageKey", null)
            if (ChangeLanguageKey)
                ChangeLanguageKey.visible = false
        }
    }
}
