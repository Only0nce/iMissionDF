# NET-STAB1 — QML Binding + Shutdown Lifecycle Hardening

Date: 2026-09-15
Base: NET-VPN2.1 (`src-NET-VPN2.1-CONTROL-PANEL-PUBLIC-IP-20260915.tar.xz`)

## Trigger evidence

Target runtime log showed three independent lifecycle faults:

1. `NetworkAccessModePopup.qml` reported `Binding loop detected for property "implicitHeight"` on the Viewer/Admin `Button` controls.
2. Shutdown reported `QProcess: Destroyed while process ("tail") is still running.`
3. After SIGTERM/graceful shutdown began, the process still faulted with SIGSEGV while the last global crash checkpoint was `AUDIO_WRITE`.

## Root cause / design correction

### 1. Viewer/Admin popup implicitHeight loop

The NET-UX3 cards used a Qt Quick Controls `Button` whose custom `contentItem` was a `ColumnLayout` with `anchors.fill: parent`.

A `Button` computes implicit size from the content item's implicit size, while the anchored content item was simultaneously deriving its size from the Button. This creates a circular implicit-height dependency on Qt 5.15.

Fix:

- preserve the existing 720x460 popup and card layout,
- use an `Item` as the Button `contentItem`,
- explicitly give the wrapper zero implicit size,
- place the existing `ColumnLayout` inside that wrapper with `anchors.fill: parent`.

The visual geometry is still controlled by the enclosing `RowLayout`; only the implicit-size dependency is removed.

### 2. LogWatcher tail process lifecycle

`LogWatcher` starts `tail -f` in a child `QProcess` but had no explicit stop/destructor path. QObject destruction therefore could destroy a still-running QProcess.

Fix:

- add `LogWatcher::~LogWatcher()`,
- add idempotent `stopWatching()`,
- `terminate()` the tail process and fall back to `kill()` if it does not exit within 500 ms,
- stop any previous tail before `startWatching()` starts another one.

### 3. AstraRX/ALSA shutdown ordering

The target log showed SIGTERM was already converted to graceful Qt shutdown, but a SIGSEGV occurred later while the audio path's last checkpoint was `AUDIO_WRITE`. The application previously relied on `WebSocketClient` member destruction to stop the audio players, which happens late in C++ stack unwinding after the Qt event loop has already ended and after other backend teardown may have begun.

Fix:

- add idempotent `WebSocketClient::shutdown()`,
- stop reconnect/status timers,
- abort the WebSocket immediately during teardown,
- join both ALSA playback worker threads,
- call `mainWindows.wsClient.shutdown()` immediately after `app.exec()` returns and before `engine.reset()` or recorder/backend object destruction,
- keep the destructor calling the same shutdown method as a second safety net.

This is a shutdown-only ordering change; normal runtime audio behavior is unchanged.

## Files changed

- `NetworkAccessModePopup.qml`
- `logwatcher.h`
- `logwatcher.cpp`
- `websocketclient.h`
- `websocketclient.cpp`
- `main.cpp`

## Deliberately unchanged

- LAN1/LAN2 Viewer permissions
- LAN3/LAN4 Viewer read-only policy
- VPN enable/status/public-IP/connect/disconnect behavior
- NetworkManager VPN profile handling
- database / Network2 / RFSoC TCP integration
- FFT/CUDA rendering path
- normal ALSA buffer/period/stage settings

## Target validation

1. Start application and open Network Access role popup repeatedly. There must be no `NetworkAccessModePopup.qml ... Binding loop detected for property "implicitHeight"` warnings.
2. Exercise recorder state so `LogWatcher` is active, then stop the application. There must be no `QProcess: Destroyed while process ("tail") is still running.` warning.
3. Keep AstraRX audio playing, then stop from Qt Creator/SSH (SIGTERM). Expected shutdown sequence includes:

   `[SIGNAL] graceful shutdown requested ...`
   `[SHUTDOWN] stopping AstraRX/audio workers before backend teardown`

   and no subsequent `R13-FATAL ... AUDIO_WRITE` crash.
4. Restart and verify analog/digital audio still initializes normally.
5. Re-test LAN/WiFi/5G/VPN navigation and VPN actions.

## Build status

The current artifact environment does not provide the target qmake/Qt 5.15 Jetson toolchain, so this revision has static/source validation only. Build and runtime validation must be performed on the project build host/Jetson.
