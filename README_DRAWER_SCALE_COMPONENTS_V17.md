V17 - Scale drawer child components with expanded drawer width

Changed files:
- iScreenDFqml/pages/SideSettingsDrawer.qml
- iScreenDFqml/sidepanels/SideLocal.qml

What changed:
1. Kept the wider drawer, but now child components scale with it.
2. Select Mode uses drawer-width-based tile sizing instead of small fixed sizes.
3. The 4-icon segment bar expands with the drawer.
4. RF Receiver Configuration input rows expand with the drawer.
5. Local Device form fields use responsive label/field widths from the current content width.
6. Local Device remains vertical-scroll only; horizontal scrolling remains disabled.
