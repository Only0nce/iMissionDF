pragma Singleton
import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15

QtObject {
    id: store
    property string currentLang: "en"

    function toggle() {
        store.currentLang = (store.currentLang === "th") ? "en" : "th"
        console.log("[LanguageStore] currentLang =", store.currentLang)
    }
    signal languageChanged(string lang)
    function tr(enText, thText) {
        return store.currentLang === "th" ? thText : enText
    }
}
