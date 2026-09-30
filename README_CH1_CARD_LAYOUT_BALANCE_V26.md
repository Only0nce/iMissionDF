V26 - CH1 card layout balance and clearer channel naming

Changed file:
- DoaViewer/TopBar.qml

Changes:
1. Keeps the V25 behavior: CH1 is Home/RX and CH2..CH6 are DF channels.
2. Makes channel cards use shared width properties:
   - channelCardWidth
   - channelCardMinWidth
3. Adds clearer target header text: "DF TARGET · CH2..CH6" because the target slider applies only to DF channels.
4. Makes the CH1 card label clearer:
   - HOME / RX FFT when RX source is available
   - HOME / RX WAIT when not available
5. Balances card width after adding CH1 so the full row is cleaner and less crowded.
