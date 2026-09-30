import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Controls.Material 2.12
import QtQuick.Layouts 1.12
import "ui"

Popup {
    id: root

    property bool darkMode: Material.theme === Material.Dark
    Theme { id: hmiTheme; darkMode: root.darkMode }

    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose

    width: 400
    height: 230
    x: parent ? Math.max(10, (parent.width - width) / 2) : 10
    y: parent ? Math.max(10, (parent.height - height) / 3) : 10

    property string titleText: "Network Settings"
    property string messageText: "Enter password to continue"
    property string unlockButtonText: "Unlock"

    property color backgroundColor: hmiTheme.panel
    property color borderColor: hmiTheme.lineStrong
    property color textColor: hmiTheme.text
    property color subTextColor: hmiTheme.textSecondary
    property color fieldColor: hmiTheme.input
    property color fieldFocusColor: hmiTheme.cardAlt
    property color accentColor: hmiTheme.accent
    property color cancelColor: hmiTheme.info
    property color errorColor: hmiTheme.danger

    property var securityBackend:
        (typeof networkSecurity === "undefined") ? null : networkSecurity

    signal authorized()
    signal cancelled()

    function requestUnlock() {
        errorText.text = ""
        passwordField.text = ""
        open()
    }

    function clearSensitiveInput() {
        passwordField.text = ""
    }

    function verifyAndContinue() {
        if (!securityBackend || typeof securityBackend.verifyPassword !== "function") {
            errorText.text = "Security service unavailable"
            clearSensitiveInput()
            return
        }

        if (securityBackend.lockedOut === true) {
            var lockedSeconds = securityBackend.lockoutRemainingSeconds
                    ? securityBackend.lockoutRemainingSeconds() : 0
            errorText.text = "Too many attempts. Try again in " + lockedSeconds + "s"
            clearSensitiveInput()
            return
        }

        if (securityBackend.verifyPassword(passwordField.text)) {
            errorText.text = ""
            clearSensitiveInput()
            close()
            authorized()
            return
        }

        var seconds = securityBackend.lockoutRemainingSeconds
                ? securityBackend.lockoutRemainingSeconds() : 0
        if (seconds > 0) {
            errorText.text = "Too many attempts. Try again in " + seconds + "s"
        } else {
            var attempts = securityBackend.remainingAttempts
                    ? securityBackend.remainingAttempts() : -1
            errorText.text = attempts >= 0
                    ? "Wrong password · " + attempts + " attempt(s) left"
                    : "Wrong password"
        }
        clearSensitiveInput()
        passwordField.forceActiveFocus()
    }

    background: Rectangle {
        radius: 14
        color: root.backgroundColor
        border.color: root.borderColor
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Text {
            text: root.titleText
            color: root.textColor
            font.pixelSize: 18
            font.bold: true
        }

        Text {
            text: root.messageText
            color: root.subTextColor
            font.pixelSize: 13
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }

        TextField {
            id: passwordField
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            echoMode: TextInput.Password
            placeholderText: "Password"
            color: root.textColor
            placeholderTextColor: root.subTextColor
            leftPadding: 10
            rightPadding: 10
            font.pixelSize: 15
            selectByMouse: true

            background: Rectangle {
                radius: 8
                color: passwordField.activeFocus
                       ? root.fieldFocusColor : root.fieldColor
                border.color: root.borderColor
                border.width: 1
            }

            Keys.onReturnPressed: root.verifyAndContinue()
            Keys.onEnterPressed: root.verifyAndContinue()
        }

        Text {
            id: errorText
            text: ""
            color: root.errorColor
            font.pixelSize: 12
            visible: text.length > 0
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                id: cancelButton
                text: "Cancel"
                Layout.fillWidth: true
                Layout.preferredHeight: 42

                background: Rectangle {
                    radius: 10
                    color: cancelButton.pressed
                           ? Qt.darker(root.cancelColor, 1.2)
                           : root.cancelColor
                }

                contentItem: Text {
                    text: cancelButton.text
                    color: root.darkMode ? "#FFFFFF" : "#061514"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.bold: true
                }

                onClicked: {
                    errorText.text = ""
                    root.clearSensitiveInput()
                    root.close()
                    root.cancelled()
                }
            }

            Button {
                id: unlockButton
                text: root.unlockButtonText
                Layout.fillWidth: true
                Layout.preferredHeight: 42

                background: Rectangle {
                    radius: 10
                    color: unlockButton.pressed
                           ? Qt.darker(root.accentColor, 1.2)
                           : root.accentColor
                }

                contentItem: Text {
                    text: unlockButton.text
                    color: root.darkMode ? "#FFFFFF" : "#061514"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.bold: true
                }

                onClicked: root.verifyAndContinue()
            }
        }
    }

    onOpened: {
        errorText.text = ""
        passwordField.text = ""
        passwordField.forceActiveFocus()
    }

    onClosed: clearSensitiveInput()
}
