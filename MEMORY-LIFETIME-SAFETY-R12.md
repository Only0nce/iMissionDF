# iScanMR10 R12 — Memory / Pointer / Lifetime Safety Hardening

Revision: `20260817-memory-lifetime-safety-r12`

## Scope

R12 is intentionally limited to unexplained `iScanMR10` process termination. It does not change the AstraRX frequency ownership algorithm or digital-codec behavior beyond what existed in R11.

The supplied source archive is incomplete relative to the Jetson build definition. `iScanMR10.pro` references **61** Jetson `.cpp` files; only **28** are present, and **33** are absent. The missing list is in `R12-MISSING-COMPILED-SOURCES.txt`. In particular, the unavailable `iRecordManage/*` files overlap runtime activity seen shortly before earlier terminations (`UnixSocket`, recorder messages), so a full executable-level safety claim is not possible yet.

## Crash-class defects hardened

### Process lifetime

- `FileUpdateWatcher` now treats an already-existing `update.tar` as baseline instead of an immediate update event.
- `Mainwindows::fileUpdated()` validates the expected package, checks each update stage, and requests `QCoreApplication::quit()` only after a successful update instead of calling raw `exit()`.
- Helper/hardware classes no longer intentionally kill the whole process for SPI initialization failure.

### Async object lifetime

- The 5G recovery worker no longer captures raw `Mainwindows *this` in `QtConcurrent::run`.
- A QObject-owned `QFutureWatcher<bool>` receives the result while `Mainwindows` is alive.
- Delayed callbacks use QObject context so Qt cancels them when the target dies.
- Several long-lived QObject members are adopted by `Mainwindows` for deterministic teardown.

### Pointer / array bounds

- `PCMImaAdpcmCodec` validates/clamps network-derived `stepIndex` **before** indexing the 89-entry IMA step table.
- ADPCM nibble values are masked at the decoder boundary.
- SigmaStudio delay parsing no longer reads `pData[length]` and register read paths validate buffer lengths.
- `DspInitWorker` no longer invokes FIR programming with a null coefficient pointer.
- I2C device path and transfer lengths are bounded; invalid fd/state cannot be reported as active.
- PCM3168A validates ADC/DAC channel indexes.
- Screenshot cache, RFDC line buffer, WebSocket binary frame size, local IPC message size, and real-time audio queue are bounded.

### Thread / ALSA ownership

- `snd_pcm_t *` is owned by the playback thread. The controlling thread does not drop/close the same raw ALSA handle while playback is using it.
- `QThread::terminate()` is not used. Shutdown signals the worker and joins it safely.
- The producer audio queue is hard-capped and discards oldest realtime audio when back-pressured rather than growing toward OOM.

### Socket lifetime

- ChatServer client wrappers are values rather than heap wrapper pointers.
- Historical body-less debug `for` statements that accidentally nested WebSocket send loops were removed, preventing client-count message multiplication.
- Socket references in wrappers use `QPointer<QWebSocket>` so deletion invalidates references automatically.
- Disconnected sockets are removed and scheduled for deletion.
- `WebSocketClient` audio-player references use QPointer and avoid double-deleting externally owned players.
- PCM software-volume processing no longer reinterprets arbitrary `QByteArray` storage as an aligned `qint16 *`; samples are loaded/stored with `memcpy` for ARM-safe defined access.

### Resource ownership

- I2C and SPI wrappers gained explicit destruction/non-copy semantics.
- Sigma/PCM/GPIO/HMC non-QObject resources owned by `Mainwindows` are released explicitly.
- Recorder socket and related QObject resources have parent ownership where available.
- Previously uninitialized recorder configuration scalars now have deterministic defaults.

## Static review coverage

Every C/C++ source/header present in the supplied archive was included in pattern-based memory/lifetime review. The review looked for:

- raw or uninitialized pointers;
- uninitialized scalar/index values;
- fixed arrays and external lengths;
- network/file data used as array indexes;
- `new/delete` ownership;
- QObject parenting and QPointer opportunities;
- async lambdas capturing raw `this`/object pointers;
- timer callbacks without context;
- thread shutdown and cross-thread raw resource access;
- socket disconnect/re-entrancy lifetime;
- unbounded queues/buffers;
- explicit process termination inside helpers;
- QML/model indexes crossing into C++ boundaries.

QML was also scanned for dynamic model/index use. QML invalid indexes usually produce QML/JS errors rather than native heap corruption, so C++ boundaries remain responsible for validation.

## Known limitations / remaining blind spots

The following cannot be closed from this archive:

1. The 33 missing Jetson-compiled source files, especially `iRecordManage/*`, `iScreenDF/*`, and `DoaViewer/DoaClient.cpp`.
2. No Qt5/qmake development toolchain exists in the current review environment, so R12 has **not** been compiled here.
3. Static checks cannot prove absence of use-after-free/data races reached only at runtime.
4. Generated/external libraries (Qt, ALSA, MariaDB, codecserver, system services) are outside this source audit.

## Build validation on the Jetson

Normal build first:

```bash
cd <R12-source>
make clean
qmake CONFIG+=release
make -j$(nproc)
```

Run the normal build and verify no compile regression before enabling sanitizers.

## ASan + UBSan diagnostic build

R12 adds an opt-in qmake configuration; production flags are unchanged unless explicitly enabled.

```bash
cd <R12-source>
make clean
qmake CONFIG+=debug CONFIG+=SANITIZE_MEMORY
make -j$(nproc)
```

Run:

```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 \
UBSAN_OPTIONS=print_stacktrace=1 \
./iScanMR10
```

On Jetson, sanitizer builds require the matching `libasan`/`libubsan` runtime and consume more RAM. They should be used for diagnosis, not production deployment.

If termination remains, the sanitizer report is more useful than a generic Qt Creator `terminated abnormally`; keep the first `AddressSanitizer` or `runtime error` stack trace intact.

## Crash runner

The existing `RUN-ISCAN-CRASH-DIAG.sh` can be used to expose the process exit code/core-dump direction without creating a log package. It does not intentionally terminate the program itself.

## Soak / stress acceptance plan

After normal and sanitizer builds pass:

1. Idle: 2 hours.
2. AstraRX analog: 2 hours.
3. D-Star + Standard SoftMBE: 2 hours.
4. AstraRX WebSocket disconnect/reconnect stress: 500 cycles.
5. Recorder/service reconnect stress: 500–1000 cycles once the missing `iRecordManage` source has been audited.
6. 5G reset while UI is active, then application close/restart during/after recovery.
7. Rapid receive-mode and page switching.
8. Malformed/truncated external packets where test harnesses are available.
9. Final 24-hour soak.

Track RSS, VmSize, thread count, file-descriptor count and client/socket count. Acceptance target is zero ASan/UBSan findings, zero `QThread: Destroyed while thread is still running`, no monotonic FD/client growth, no unbounded queue growth, and no unexplained process termination.
