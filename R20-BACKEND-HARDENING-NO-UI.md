# R20 Backend Hardening — No UX/UI Changes

Date: 2026-09-09

## Scope contract

This revision is intentionally backend-only. No `.qml` file, QML object name,
menuID, layout, visual style, navigation flow, or user interaction contract is
changed.

## Implemented fixes

1. **ChatServer single generic dispatch**
   - Reject malformed JSON before routing.
   - A normal inbound JSON command reaches `newCommandProcess` once.
   - Prevent duplicate system/VU socket list entries.
   - Recalculate client count on connect/disconnect.
   - Release disconnected `QWebSocket` objects with `deleteLater()`.

2. **Timezone command hardening**
   - Validate timezone against `QTimeZone::availableTimeZoneIds()`.
   - Remove shell string construction from `setLocation()`.
   - Invoke `ln -sf` through `QProcess` argv.

3. **Non-blocking RF scan**
   - Preserve the original `-1.6 MHz .. +1.6 MHz`, 1 kHz step and 500 ms
     cadence.
   - Replace the GUI-thread blocking loop/sleep with a `QTimer` state machine.

4. **AstraRX reconnect**
   - Backend-owned reconnect with bounded 1/2/4/8/15 second backoff.
   - Clear stale local audio on disconnect.
   - Keep the existing QML-facing WebSocket API unchanged.

5. **Frequency transaction guard**
   - Existing QML writers still call `Mainwindows::sendmessage()` unchanged.
   - A DSP `offset_freq` that races a pending `setfrequency` is deferred until
     AstraRX confirms the requested center.
   - A 2 second timeout preserves the old eventual-send behavior.

6. **ALSA lifecycle/recovery**
   - Playback thread is the sole owner of the ALSA handle.
   - Remove `QThread::terminate()` fallback.
   - Hard-bound producer queue and drop stale oldest audio under overload.
   - Retry ALSA initialization instead of leaving a dead consumer.
   - Start playback lazily on first audio packet.
   - Preserve the historical default timing: 100 ms buffer / 20 ms period /
     100 ms stage and a 30-chunk queue ceiling. Lower-latency values remain
     available only as explicit environment overrides for controlled A/B tests.

7. **ADPCM safety**
   - Remove an uninitialized decoder pointer.
   - Clamp synchronized ADPCM step index before fixed-table access.
   - Mask decoder nibble before index-table access.
   - Test tone now uses the active HD player sample rate.

8. **NetworkController ownership**
   - Reuse the same `NetworkController` instance for QML and `Mainwindows`.
   - Remove the duplicate backend controller instance.

9. **5G async lifetime**
   - Worker no longer captures/dereferences raw `this` during background reset.
   - Completion callback is guarded by `QPointer`.

10. **I2C safety**
    - File descriptor starts at `-1`.
    - Do not call `ioctl()` after open failure.
    - Do not mark failed I2C initialization active.
    - Bound-copy device path and close descriptor in destructor.
    - Reject invalid read/write lengths and inactive descriptors.

11. **Deferred recorder-config lifetime**
    - `QTimer::singleShot` now uses the manager QObject as its context.

12. **Dormant DSP init null-FIR crash**
    - Removed calls that passed `nullptr` to `setFIRfilter()`; the implementation
      dereferences the coefficient pointer.

## Validation performed in this source package

- `git diff --check`: PASS
- QML files changed from baseline: **0**
- `QThread::terminate()` in active ALSA implementation: **0**
- Active null `setFIRfilter(..., nullptr)` calls: **0**
- Generic `emit newCommandProcess(...)` in `ChatServer::commandProcess`: **1**
- Blocking 3,200-step/500 ms scan loop: removed
- `setLocation()` shell-string execution: removed

A full qmake build was not possible in the review container because Qt5/qmake
and several source/resource directories referenced by `iScanMR10.pro` are not
present in the supplied archive. Target hardware validation is therefore still
required before production deployment.

## Additional risks intentionally left for the next stage

These were not silently changed because they affect deployment, hardware
failure behavior, protocol security, or update semantics and need targeted
validation:

1. ChatServer is still `NonSecureMode`, binds `QHostAddress::Any`, and has no
   backend authentication boundary before privileged commands.
2. `SPIClass::spi_init()` calls `exit(-1)` on device/config failure, terminating
   the whole GUI process instead of reporting a recoverable hardware fault.
3. Firmware upload watcher reacts as soon as `update.tar` appears/changes; it
   can try to extract a partially uploaded archive, remove it, and exit the app.
   There is no stable-file/complete-upload transaction or cryptographic
   authenticity check in this path.
4. The actual audio architecture still owns two ALSA players (12 kHz analog,
   16 kHz digital). Lazy opening reduces contention but does not establish the
   documented single-owner 48 kHz architecture when both streams are active.
5. `WebSocketClient::setHdAudioPlayer()` has ambiguous ownership: replacing the
   internally allocated pointer can leak the old player and the destructor can
   delete an externally owned replacement.
6. `RfdcNcoClient::m_rxBuffer` has no maximum size when a peer sends data without
   a newline, allowing unbounded growth.
7. Screenshot `ImageProvider::m_images` is never evicted; repeated captures can
   grow memory for the lifetime of the process, and `sender()` is cast to
   `QQuickWindow*` without runtime type/null validation.
8. Database credentials are embedded in source/binary and DB open failure can
   restart MySQL from inside the application. Credentials should be externalized
   and rotated; service recovery should belong to systemd/supervision.
9. Multiple legacy `system()` command paths remain elsewhere in the tree. Each
   must be traced to its input source before deciding whether it is command
   injection or only a constant command; migration should use argv-based
   `QProcess` calls.
10. Local IPC and some socket receive paths use `readAll()`/accumulation without
    an explicit protocol payload ceiling.
11. Startup GPIO timing contains a 500 ms sleep next to a comment requiring
    more than 900 ms. This needs schematic/hardware-sequence verification before
    changing it.
12. The supplied archive is incomplete relative to `iScanMR10.pro`/`qml.qrc`,
    so cross-module ABI/resource/build regressions cannot be fully ruled out in
    this environment.
