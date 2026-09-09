# Qt5 / AstraRX Codec + Crash Hardening R11

Revision: `20260817-codec-crash-hardening-r11`

## Qt stability changes

- Frequency updates no longer call `onSQLChanged()`. Frequency state and squelch/GPIO state have separate ownership.
- Confirmed AstraRX receiver frequency is coalesced to 100 ms before propagation to recorder/QML consumers.
- Repeated unchanged SQL state no longer re-reads/writes SHD_AMP/HS_MUTE/LED GPIO.
- ALSA producer queue is hard-bounded at 30 chunks; oldest audio is dropped instead of allowing memory growth if ALSA stalls.
- `QThread::terminate()` was removed from the audio shutdown path. Shutdown now requests stop, drops ALSA, wakes waiters, and joins the thread before destruction.
- The legacy PCM IMA ADPCM helper no longer dereferences an uninitialized decoder pointer; decoder state is persistent in `WebSocketClient`.
- Recorder WebSocket re-registration replaces stale wrappers instead of accumulating duplicate socket references.
- Disconnected WebSocket objects use `deleteLater()` after all lists/wrappers release them.
- Cross-thread recorder sends capture a `QPointer<QWebSocket>` so queued sends cannot dereference a socket deleted before delivery.
- Mainwindows adopts previously unparented QObject services/controllers and the scan timer now has QObject ownership.

## Crash capture helper

Run:

```bash
./RUN-ISCAN-CRASH-DIAG.sh /opt/iScanMR10/bin/iScanMR10
```

The helper does not package logs. It enables core dumps, runs the binary in the foreground, prints the real exit code, prints matching kernel faults when permitted, and shows the commands required to open the latest core with `coredumpctl gdb`.
