import QtQuick 2.15

QtObject {
    id: root

    readonly property bool available: (typeof translationManager !== "undefined" && translationManager)
    property string language: available ? translationManager.language : "en"

    function tr(source) {
        // Make QML bindings re-evaluate when language changes.
        language

        if (available && translationManager.text)
            return translationManager.text(String(source))
        return String(source)
    }
}
