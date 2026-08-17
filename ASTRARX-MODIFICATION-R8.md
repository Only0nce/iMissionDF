# AstraRX Qt5 Integration R8

Authoritative input: `src_wifi_17082026.tar.xz`  
Revision: `20260817-src-wifi-astrarx-r8`

## Ownership contract

- AstraRX owns the RF/source center frequency and RFSoC hardware tuning.
- Qt/QML is a WebSocket client only for AstraRX tuning.
- `centerFreq` is the RF/source center.
- `offsetFrequency` is the per-client DSP receiver offset.
- The large receiver readout remains the original project architecture: `freqScan = centerFreq + offsetFrequency`.
- Normal tuning inside the active IQ span sends only `dspcontrol.params.offset_freq`.
- Intentional out-of-span recenter, memory selection, or scan-center movement may send `setfrequency` through the AstraRX WebSocket.
- The legacy Qt TCP `SetFreqWorker` / RFSoC port 6000 path is removed from the build.

## Frequency changes

1. `WebSocketClient` now stores `start_freq` and normalizes partial `center_freq`, `start_freq`, and `start_offset_freq` config messages into one consistent receiver snapshot.
2. Center/source metadata and receiver tuning are separate events:
   - `updateCenterFreq()` = center/sample-rate/mode metadata.
   - `receiverStateChanged(center, offset, receiver)` = absolute receiver state.
3. `Mainwindows` bridges the atomic receiver snapshot to QML with `updateReceiverFreq(...)`.
4. `SpectrumGLPlot.qml` keeps the original `centerFreq + offsetFrequency -> freqScan` model. No new `receiverFrequency` property was introduced.
5. Backend assignments use `backendFrequencySync` so they update the UI without echoing offset commands back to AstraRX.
6. `updateCenterFreq()` no longer writes `freqScan = centerFreq`.
7. The legacy `resetCenterFreqTimer` / 30 MHz bounce is removed.
8. Spectrum click/drag has one offset-command path and keeps the full demod passband inside the IQ span.
9. `RadioScanner.qml` seeds the readout from the backend receiver snapshot and unit changes only change formatting.
10. `HomeDisplay.qml` restores local volume/display preferences after connection, but does not overwrite AstraRX RF/DSP state from stale local OpenWebRX configuration.

## Squelch

AstraRX `{"type":"squelch","value":...}` is handled explicitly. Once explicit state is seen it becomes authoritative. Audio-packet timeout inference remains only as compatibility fallback.

## Waterfall

- Default view: -130 to -80 dB.
- 13-stop dark-blue to white palette.
- Robust percentile auto contrast with bounded dynamic range and smoothing.
- Peak-per-display-column aggregation preserves narrow carriers.
- Manual scale movement disables auto contrast for that session.

## Audio / shutdown

- SD 12 kHz and HD 16 kHz playback buffer, period and pacing are calculated from each player's actual sample rate.
- `m_running` is atomic across owner/audio threads.
- `AlsaAudioPlayer` is no longer moved into the QThread it owns; only `audioLoop()` executes there through a direct connection.
- Shutdown wakes the loop, drops a blocking ALSA stream if needed, quits and waits for the playback thread.
- `WebSocketClient` stops/deletes both audio players during teardown.
- `SetFreqWorker` and its always-running QThread are removed, eliminating the strongest source of `QThread: Destroyed while thread is still running` from this source tree.

## Runtime markers

Expected at startup:

```text
[ASTRARX-COMPAT-BUILD] revision=20260817-src-wifi-astrarx-r8
[ASTRARX-BACKEND] WebSocket = ...
[ASTRARX-BACKEND] direct RFSoC SetFreqWorker disabled
qml: [ASTRARX-COMPAT-QML] revision=20260817-src-wifi-astrarx-r8
```

Normal in-span tuning example:

```text
RF center = 96.457600 MHz
selected receiver = 99.000000 MHz
offset = +2.542400 MHz
```

Expected result:

```text
AstraRX RF center: 96.457600 MHz
Qt large readout: 99.000000 MHz
Qt TX: dspcontrol offset_freq=2542400
```

No Qt TCP port-6000 RF retune should occur.

## Build note

The uploaded source snapshot does not contain all directories referenced by `iScanMR10.pro` (for example `iScreenDF`, `iRecordManage`, and `DoaViewer`), and the validation container does not have Qt5/qmake. Source/static validation is complete, but build/runtime validation must be performed after these files are overlaid into the complete target source tree.
