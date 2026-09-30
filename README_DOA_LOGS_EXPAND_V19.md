V19 - Expand DOA Logs component + fix primary nav icon fallback

Changed files:
- MainPage.qml
- iScreenDFqml/pages/SideSettingsDrawer.qml
- iScreenDFqml/sidepanels/SideLogsFile.qml

What changed:
1. When the bottom side panel is set to Data Logs, the RF Receiver Configuration block becomes more compact.
   - VFO form switches from 2-column vertical rows to a 4-column compact layout.
   - This frees more vertical room for the DOA Logs component.
2. The side panel container has a larger minimum height when Data Logs is active.
3. SideLogsFile now has a wider/spacious mode based on available width.
   - Larger DOA Logs title and Refresh DOA button.
   - Taller search/paging/control rows.
   - Larger list rows and text where there is enough space.
4. Fixed MainPage primary navigation iconSource fallback for the new iconLight/iconDark model entries.
   - This prevents "Unable to assign [undefined] to QUrl" when modelData.icon is no longer present.
5. No backend/database/map selection logic changed.
