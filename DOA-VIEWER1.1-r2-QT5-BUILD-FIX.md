# DOA-VIEWER1.1-r2 — Qt 5.15.2 Build Fix

This is a surgical build-compatibility revision on top of DOA-VIEWER1.1.
No RF/DF mapping, FFT acquisition, MUSIC/Polar, TCP protocol, or rendering behavior is changed.

## Fixed hard build failures

1. `Mainwindows.h` / moc
   - Removed the five newly-added DoA RX `Q_PROPERTY` declarations from the large legacy `Mainwindows` class.
   - Kept the same data as `Q_INVOKABLE` getters and retained `doaRxFftFrameChanged()` as the update trigger.
   - `ViewerPage.qml` now reads the snapshot by calling the getters.
   - Added an explicit `<QVariantList>` include.

2. `main.cpp` calling `mainWindows.wsClient.shutdown()`
   - Added an idempotent `WebSocketClient::shutdown()` API.
   - Stops reconnect and SQL timers first, clears the reconnect target, stops audio players, then closes the WebSocket.
   - Destructor reuses the same shutdown path.

## Intentionally unchanged

- CH1 = AstraRX/Home RX Spectrum/Waterfall source.
- CH2..CH6 = DF physical CH1..CH5 mapping.
- MUSIC / Polar coherent 5-channel calculation.
- Network/Endpoints behavior.
- Existing legacy compiler warnings unrelated to these two hard errors.
