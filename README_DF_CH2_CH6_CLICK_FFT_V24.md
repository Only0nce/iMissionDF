V24 - DF cards relabeled CH2..CH6 and clickable FFT channel selector

Changed files:
- DoaViewer/TopBar.qml

What changed:
1. Renamed the RF AGC/DF card header from "TARGET · DF CH1..CH5" to "TARGET · DF CH2..CH6".
2. Renamed each displayed DF card from CH1..CH5 to CH2..CH6.
3. Preserved backend physical DF indexing as 0..4, so no RF/AGC protocol changes are made.
4. Made each DF card clickable. Clicking a card requests the matching logical FFT display channel:
   - DF backend index 0 -> UI FFT CH2
   - DF backend index 1 -> UI FFT CH3
   - DF backend index 2 -> UI FFT CH4
   - DF backend index 3 -> UI FFT CH5
   - DF backend index 4 -> UI FFT CH6
5. Added selected/hover visual state to show which DF card is currently used by FFT.

Notes:
- CH1 remains the shared Home/RX FFT source.
- CH2..CH6 map to the existing five DF physical channels.
- This patch is UI/QML only and does not change backend protocol, RF control, or AGC math.
