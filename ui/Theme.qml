// Theme.qml
import QtQuick 2.15

QtObject {
    // ===== Base surfaces =====
    property color bg:            "#0F1115"
    property color panel:         "#1C1F26"
    property color panelBorder:   "#2E3440"

    // ===== Text =====
    property color text:          "#E5E5E5"
    property color subtext:       "#8A8F9A"
    property color muted:         "#6B7280"

    // ===== Accents =====
    property color accent:        "#00FF99"
    property color accentHover:   "#19E6A1"
    property color warning:       "#FFAA33"
    property color danger:        "#FF5555"

    // ===== Spectrum/Waterfall =====
    property color gridLine:      "#444444"
    property color axisText:      "#AAAAAA"
    property color spectrumLine:  "#00FF00"
    property color maxHoldLine:   "#E5C700"

    // ใช้เป็น “สตริง” ไปเลยเมื่ออยู่ใน Canvas 2D
    property string selectionFillCss: "rgba(255,255,255,0.15)"
    property color  cursorLine:   "#FFFFFF"
    property string zoomViewportCss: "rgba(255,255,255,0.50)"
}
