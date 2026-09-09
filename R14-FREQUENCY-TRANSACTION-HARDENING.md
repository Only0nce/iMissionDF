# R14 — Frequency Transaction Hardening

## Scope

R14 targets the Qt/QML frequency state path only. It keeps the R13.2 crash diagnostics and the R12/R11 behavior outside frequency ownership unchanged.

The change is motivated by repeated SIGSEGV failures observed inside the Qt5 QML runtime at the same relative instruction offset while different main-thread checkpoints were active. R14 therefore removes re-entrant and multi-writer frequency state transitions rather than applying another audio-side workaround.

## Production ownership model

AstraRX remains authoritative for confirmed RF/source and receiver state.

- In-span receiver request: Qt sends only `dspcontrol.offset_freq`.
- Out-of-span receiver request: Qt sends only `setfrequency`, keeps the previous confirmed center/offset/readout, and waits for AstraRX confirmation.
- Preset with center + offset: Qt first requests the source center, waits for AstraRX confirmation, then sends the preset offset.
- AstraRX-originated center changes: Qt applies center, offset, receiver, sample rate, and mode as one QML state transaction.

## Main changes

### 1. One C++ -> QML frequency snapshot

`WebSocketClient` now emits:

`frequencyStateChanged(centerHz, offsetHz, receiverHz, sampleRate, mode)`

for any center/sample-rate/mode/receiver change.

`Mainwindows` consumes that signal and emits one matching QML signal. The legacy `updateCenterFreq` and `updateReceiverFreq` signals remain in the API but are no longer connected to `SpectrumGLPlot`.

This removes the previous sequence where one AstraRX config could trigger a center update and then a second receiver update through separate QML call chains.

### 2. Atomic QML commit

`SpectrumGLPlot` uses explicit state values instead of implicit bindings for:

- `centerFreq`
- `sampRate`
- `offsetFrequency`

`applyBackendFrequencyState()` raises `frequencyStateApplying`, installs the complete state, synchronizes dependent mode/bandwidth properties, lowers the guard, then performs one readout/repaint/update pass.

Property handlers do not repaint or send commands while an atomic backend transaction is being committed.

### 3. Single frequency command owner

Normal UI code no longer writes `spectrumGLPlot.centerFreq` or `offsetFrequency` directly and no longer sends `setfrequency` directly.

The central APIs are:

- `requestReceiverFrequency(targetHz, source)`
- `requestSourceCenter(targetCenterHz, reason, postTuneOffsetHz)`
- `requestPresetFrequency(centerHz, offsetHz, source)`

Updated callers include:

- `MemoryAddEdit.qml`
- `NewMemoryAddEdit.qml`
- `LogDeviceScanner.qml`
- `ReceiveModeScanner.qml`
- `RadioScanner.qml` already enters through `setManualOffset()` / the central receiver API.

### 4. No stale offset after recenter

Every frequency request has a local generation number.

When `setfrequency` is requested:

- the offset coalescing timer is stopped;
- the old pending offset is discarded;
- touch/scan offset writers cannot send during the center transaction;
- a newer center request supersedes an older in-flight request;
- a snapshot for a different center is ignored while waiting for the current requested center;
- delayed `Qt.callLater` post-tune offsets verify their generation before sending.

### 5. Preset center + offset ordering

Preset DSP commands no longer include `offset_freq` in the delayed general DSP payload.

Instead:

1. request preset RF center;
2. wait for AstraRX center confirmation;
3. send preset offset if it is inside the confirmed usable span;
4. wait for AstraRX receiver snapshot;
5. commit the confirmed offset/readout.

This prevents a 500 ms preset timer from applying an offset calculated for the previous RF center.

### 6. Peak scan waits for hardware confirmation

Peak scan no longer performs:

`centerFreq = ...; offsetFrequency = 0; setfrequency; offset_freq=0`

optimistically.

`tuneToCenter()` now requests the center and waits for `onFrequencySnapshotCommitted()` before starting settle/dwell timing. Stop/finish restoration follows the same server-confirmed path.

### 7. Frequency diagnostics retained

R13.2 diagnostics remain enabled. R14 additionally persists low-rate events for:

- outbound `setfrequency` requests;
- confirmed AstraRX source-center snapshots.

Rapid in-span offsets are not persisted individually, avoiding diagnostic I/O pressure during touch drag.

## Expected transaction examples

### In span

Current center 88 MHz, target 90 MHz:

`requestReceiverFrequency(90 MHz)` -> `offset_freq=+2 MHz` -> AstraRX receiver snapshot -> QML commit.

RF center remains 88 MHz.

### Out of span

Current center 88 MHz, target 95 MHz:

`requestReceiverFrequency(95 MHz)` -> `setfrequency=95 MHz` -> no optimistic center change -> AstraRX confirms center=95 MHz, offset=0 -> one QML snapshot commit.

### Superseded out-of-span request

95 MHz is requested, then 101 MHz is requested before 95 MHz is confirmed:

- generation N sends center 95 MHz;
- generation N+1 sends center 101 MHz;
- an intervening 95 MHz snapshot is ignored by QML;
- the 101 MHz confirmation completes the current transaction.

### Preset

Preset center 100 MHz, offset +250 kHz:

- `setfrequency=100 MHz`;
- wait for confirmed center 100 MHz;
- send `offset_freq=250000`;
- commit receiver 100.250 MHz when AstraRX confirms.

## Validation performed here

`VERIFY-FREQUENCY-TRANSACTION-R14.sh` checks:

- atomic WebSocket/Mainwindows/QML signal path;
- no legacy frequency signal connections in SpectrumGLPlot;
- no implicit `onSetCenterFreqChanged` network writer;
- no external direct center/offset assignment;
- no direct QML `setfrequency` sender outside SpectrumGLPlot;
- stale-generation and stale-snapshot guards;
- preset and peak-scan ownership;
- structural balance of all 50 QML files.

Static result: 24 PASS / 0 FAIL.

The current environment does not contain the project's Qt5/qmake cross-build toolchain, so the final Qt compile and Jetson runtime test must be performed on the target development environment.
