# R17 FFT / Audio Algorithm Hardening

## Objective

Localize and harden the IQ-derived display/audio data path without changing AstraRX protocol semantics or the normal UI defaults.

## Changes

### FFT ingress validation

Primary FFT type-1 frames now pass through a native validation stage before QVariant/QML delivery:

- Uncompressed payload size must be a multiple of `sizeof(float)`.
- Bin count must be `> 0` and `<= ISCAN_FFT_MAX_BINS` (default 65536).
- When `ISCAN_FFT_STRICT_SIZE=1` (default), bin count must match `rxconfig.fft_size` when the server supplied a positive `fft_size`.
- Every native float must be finite. Frames containing NaN/Inf are rejected as a whole.
- `QVariantList` boxing happens only after the native frame passes validation.

Rejected frames are logged as `[R17 FFT REJECT]` with bounded log frequency.

### Spectrum safety

- Spectrum coordinates sanitize `NaN/Inf` and clamp values into the active dB range.
- Viewport mapping guarantees `startBin` and `endBin` remain valid distinct indices for frames with at least two bins.
- The previous right-edge expression could produce `endBin == fullBins`.

### Waterfall isolation

Waterfall rendering can be disabled independently. When disabled it does not retain the latest FFT array and does not execute the self-scroll `drawImage()` path.

### Persistent HD ADPCM state

HD ADPCM now uses the existing persistent `m_pcmAdpcmDecoder` instead of creating a new decoder for every WebSocket packet. Decoder state is reset on backend reconnect and when audio compression mode changes.

### Runtime isolation controls

Defaults are all ON and preserve the normal UI:

```text
ISCAN_FFT_QML_PUBLISH=1
ISCAN_SPECTRUM_PAINT=1
ISCAN_WATERFALL_PAINT=1
ISCAN_FFT_STRICT_SIZE=1
ISCAN_FFT_MAX_BINS=65536
```

The properties are also exposed on `wsClient` and can be changed at runtime with:

```qml
wsClient.setFftQmlPublishEnabled(false)
wsClient.setSpectrumPaintEnabled(false)
wsClient.setWaterfallPaintEnabled(false)
```

`wsClient.fftDiagnostics()` returns counters for FFT receive/publish/suppress/reject and binary types 0..4.

## A/B Isolation Matrix

### Test A — receive FFT but never cross into QML

```bash
export ISCAN_FFT_QML_PUBLISH=0
export ISCAN_SPECTRUM_PAINT=0
export ISCAN_WATERFALL_PAINT=0
/opt/iScanMR10/bin/iScanMR10
```

Audio and the WebSocket remain active. Type-1 uncompressed frame shape is still validated before being suppressed.

### Test B — Spectrum only

```bash
export ISCAN_FFT_QML_PUBLISH=1
export ISCAN_SPECTRUM_PAINT=1
export ISCAN_WATERFALL_PAINT=0
/opt/iScanMR10/bin/iScanMR10
```

### Test C — Waterfall only

```bash
export ISCAN_FFT_QML_PUBLISH=1
export ISCAN_SPECTRUM_PAINT=0
export ISCAN_WATERFALL_PAINT=1
/opt/iScanMR10/bin/iScanMR10
```

### Test D — production-equivalent default

```bash
unset ISCAN_FFT_QML_PUBLISH
unset ISCAN_SPECTRUM_PAINT
unset ISCAN_WATERFALL_PAINT
unset ISCAN_FFT_STRICT_SIZE
unset ISCAN_FFT_MAX_BINS
/opt/iScanMR10/bin/iScanMR10
```

## Interpretation

- A stable, B unstable: prioritize Spectrum/QML Canvas path.
- A stable, C unstable: prioritize Waterfall Canvas/self-scroll path.
- A unstable: the direct Spectrum/Waterfall render algorithms are not required to trigger the crash; investigate Qt/QML events outside FFT rendering or upstream native corruption.
- Repeated `[R17 FFT REJECT]`: inspect AstraRX frame construction/`fft_size` contract before changing the UI again.
