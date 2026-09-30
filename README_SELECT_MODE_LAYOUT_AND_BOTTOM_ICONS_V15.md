V15 - Select Mode layout cleanup + bottom 4-icon bar theme tint

Changes:
1. Adjusted Select Mode button layout to prevent text overflow and overlapping.
   - Wider/taller tiles
   - Smaller icon and wrapped labels
   - Two-line labels centered cleanly
2. Updated the 4-icon bottom segmented bar to tint icons by theme.
   - Dark mode icons become white for higher contrast
   - Light mode icons follow theme text/active colors
3. Slightly increased bottom bar height for cleaner spacing.

Changed files:
- iScreenDFqml/pages/SideSettingsDrawer.qml
- iScreenDFqml/pages/PillSegmentBar.qml
