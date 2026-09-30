V20 - DOA Logs height-only correction

User correction:
- Revert the previous DOA Logs internal UI enlargement from V19.
- The intended change was only to make the DOA Logs component slightly taller.

Changed behavior:
1. Restored SideLogsFile.qml to the pre-V19 layout and sizing.
2. Restored RF Receiver Configuration layout to the pre-V19 normal layout.
3. Kept the MainPage icon fallback fix from V19 to prevent undefined QUrl warnings.
4. Added only a modest minimum height for the side panel container when sidePanelKey == "datalogs".

Changed files:
- iScreenDFqml/pages/SideSettingsDrawer.qml
- iScreenDFqml/sidepanels/SideLogsFile.qml

Kept from V19:
- MainPage.qml fallback for iconLight/iconDark remains unchanged.
