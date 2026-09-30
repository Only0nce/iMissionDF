// Shared iScanMR10 HMI design tokens.
// Keep this object lightweight: pages may instantiate it locally and feed the
// inherited Material light/dark state through darkMode.
import QtQuick 2.15

QtObject {
    property bool darkMode: true

    // ===== Geometry =====
    readonly property int topBarHeight: 60
    readonly property int navigationRailWidth: 76
    readonly property int controlHeight: 46
    readonly property int radioCommandBarHeight: 72
    readonly property int radioFrequencyCardWidth: 390
    readonly property int radiusXs: 4
    readonly property int radiusSm: 7
    readonly property int radiusMd: 10
    readonly property int radiusLg: 14
    readonly property int gapXs: 4
    readonly property int gapSm: 8
    readonly property int gapMd: 12
    readonly property int gapLg: 18

    // ===== Responsive / accessibility =====
    // Keep touch targets usable on the embedded display while allowing the
    // shell to adapt when the application is run in a smaller desktop window.
    readonly property int minTouchTarget: 44
    readonly property int compactWidth: 1280
    readonly property int narrowWidth: 1000
    readonly property int compactNavigationRailWidth: 68

    // ===== Primary HMI surfaces =====
    readonly property color page:          darkMode ? "#102221" : "#F4F7F7"
    readonly property color shell:         darkMode ? "#152927" : "#FFFFFF"
    readonly property color topBar:        darkMode ? "#203632" : "#FFFFFF"
    readonly property color sidebar:       darkMode ? "#172622" : "#FFFFFF"
    readonly property color panel:         darkMode ? "#203733" : "#FFFFFF"
    readonly property color card:          darkMode ? "#263F3A" : "#FFFFFF"
    readonly property color cardAlt:       darkMode ? "#1B302C" : "#EEF4F3"
    readonly property color input:         darkMode ? "#10201E" : "#F8FBFB"

    // ===== Typography =====
    readonly property color text:          darkMode ? "#F1F6F3" : "#172624"
    readonly property color textSecondary: darkMode ? "#C4D2CE" : "#384D49"
    readonly property color muted:         darkMode ? "#879995" : "#697B77"

    // ===== Lines / semantic colors =====
    readonly property color line:          darkMode ? "#3A5550" : "#C9D8D4"
    readonly property color lineStrong:    darkMode ? "#55736C" : "#9FB4AE"
    readonly property color accent:        darkMode ? "#12A487" : "#008B75"
    readonly property color accentHover:   darkMode ? "#4FC3A7" : "#16A98F"
    readonly property color accent2:       accentHover
    readonly property color success:       darkMode ? "#43D17E" : "#16824C"
    readonly property color warning:       darkMode ? "#F2A93B" : "#B97910"
    readonly property color danger:        darkMode ? "#FF6269" : "#C73340"
    readonly property color info:          darkMode ? "#67A8FF" : "#2167BC"
    readonly property color disabled:      darkMode ? "#42524F" : "#D5DFDD"

    // ===== RF analyzer =====
    // Analyzer follows the global HMI theme. Dark mode keeps the proven
    // instrument palette; Light mode uses high-contrast off-white plot
    // surfaces so Spectrum/Waterfall/Polar/Waveform no longer appear as dark
    // islands inside an otherwise light application.
    readonly property color plot:           darkMode ? "#071011" : "#F8FBFC"
    readonly property color gridLine:       darkMode ? "#2D4643" : "#D7E3E1"
    readonly property color gridLineStrong: darkMode ? "#48645E" : "#AFC4BF"
    readonly property color axisText:       darkMode ? "#C6D9D5" : "#314743"
    readonly property color spectrumLine:   darkMode ? "#B7FF3C" : "#149447"
    readonly property color maxHoldLine:    darkMode ? "#FF3B2F" : "#D73732"
    readonly property color analyzerPanel:  darkMode ? "#0D1721" : "#FFFFFF"
    readonly property color analyzerHud:    darkMode ? "#CC0D1721" : "#F2F7F6"
    readonly property color analyzerBorder: darkMode ? "#263948" : "#B8CCC7"
    readonly property color analyzerAccent: darkMode ? "#35D5BD" : "#008B75"
    readonly property color analyzerText:   darkMode ? "#EEF6FB" : "#18302C"
    readonly property color analyzerMuted:  darkMode ? "#91A4B3" : "#617772"

    // ===== Navigation / drawer readability =====
    // Light mode needs real contrast instead of merely lowering opacity.
    // These tokens are used by SideSettingsDrawer, navigation buttons, and
    // remote/group delegates so data stays visible in both themes.
    readonly property color navTile:           darkMode ? "#203733" : "#FFFFFF"
    readonly property color navTileHover:      darkMode ? "#2B4742" : "#EAF4F2"
    readonly property color navTileActive:     darkMode ? "#12A487" : "#00D6C2"
    readonly property color navTileBorder:     darkMode ? "#55736C" : "#9FB4AE"
    readonly property color navTileText:       darkMode ? "#F1F6F3" : "#172624"
    readonly property color navTileSubText:    darkMode ? "#C4D2CE" : "#384D49"
    readonly property color navTileActiveText: "#062826"
    readonly property real  navIconOpacity:    darkMode ? 0.86 : 1.0
    // Toolbar PNG icons are mostly pale line art. In light mode tint them to
    // a high-contrast stroke color so the six Select Mode icons remain clear
    // on white/off-white surfaces. Dark mode keeps the original artwork.
    readonly property color navIconStroke:     darkMode ? "#F1F6F3" : "#24413C"
    readonly property color navIconStrokeHover: darkMode ? "#FFFFFF" : "#008B75"

    readonly property color remoteRow:         darkMode ? "#243B36" : "#FFFFFF"
    readonly property color remoteRowHover:    darkMode ? "#2D4943" : "#EAF4F2"
    readonly property color remoteRowActive:   darkMode ? "#1B4B43" : "#DFF6F2"
    readonly property color remoteRowBorder:   darkMode ? "#3A5550" : "#B8CCC7"
    readonly property color remoteRowBorderActive: darkMode ? "#4FC3A7" : "#008B75"

    // ===== Legacy aliases used by existing pages =====
    readonly property color bg: page
    readonly property color panelBorder: line
    readonly property color subtext: textSecondary
    readonly property string selectionFillCss: darkMode ? "rgba(255,255,255,0.15)" : "rgba(0,139,117,0.12)"
    readonly property color cursorLine: darkMode ? "#FFFFFF" : "#008B75"
    readonly property string zoomViewportCss: darkMode ? "rgba(255,255,255,0.50)" : "rgba(0,139,117,0.35)"
}
