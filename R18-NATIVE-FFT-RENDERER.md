# R18 Native FFT Renderer

## Purpose

R18 removes the production Spectrum/Waterfall FFT hot path from QML/QV4.
The recurring SIGSEGV signature was on the main/QML side after WebSocket
binary processing had already returned. R17 validated incoming FFT data but
still sent full 8192-bin QVariant lists into QML when the normal display path
was enabled.

## Architecture

Production path:

```text
AstraRX FFT binary
        |
        v
WebSocketClient
  - strict size/alignment/finite validation
  - full-resolution QVector<float> scratch
  - full-resolution native Max Hold / Peak Scan state
        |
        +--> spectrum peak reducer --> <= 8192 QVector<float>
        |                              |
        |                              v
        |                       FftDisplayItem (Spectrum)
        |
        +--> waterfall reducer --> <= 1280 QVector<float>
                                       |
                                       v
                                FftDisplayItem (Waterfall)
```

No FFT-sized QVariantList is required by the normal Spectrum/Waterfall UI.
`ISCAN_FFT_QML_PUBLISH` is retained only as a legacy diagnostic/compatibility
bridge and defaults to `0` in R18.

## Full-resolution behavior preserved

- Incoming FFT validation still uses the original FFT bin count.
- Native Max Hold still accumulates every full-resolution FFT bin.
- `maxHoldSnapshot()` still returns the full-resolution snapshot on demand for
  Peak Scan. It is not continuously pushed into QML.
- Frequency mapping, zoom, pan, center/offset ownership and AstraRX protocol are
  unchanged.
- Audio paths are unchanged from R17.

## Native renderer

`FftDisplayItem` is a C++ `QQuickPaintedItem` registered as:

```qml
import iScan.Display 1.0
```

Two instances replace the old FFT Canvas hot paths:

- `FftDisplayItem.Spectrum`
- `FftDisplayItem.Waterfall`

The existing QML grid and overlay Canvas items remain because they do not carry
per-frame FFT arrays.

Frame/image state is protected by a mutex because `paint()` may execute on the
Qt Quick render thread while WebSocket display frames are delivered on the GUI
thread.

## Display reduction

Default production limits:

```text
Spectrum:  8192 bins
Waterfall: 1280 bins
```

If the source FFT has 8192 bins, the default Spectrum path keeps all 8192
bins so zoom fidelity is unchanged. The native renderer then samples only the
points needed by the current pixel width. If a larger FFT is configured, the
Spectrum transport is peak-pooled to the configured display limit.

Waterfall uses peak pooling to the previous 1280-column display budget, so
narrow RF carriers are not averaged away. The source FFT remains
full-resolution in C++ in every case.

Optional environment overrides:

```text
ISCAN_SPECTRUM_DISPLAY_BINS=<positive integer>
ISCAN_WATERFALL_DISPLAY_BINS=<positive integer>
```

Values are capped by `ISCAN_FFT_MAX_BINS`.

## Legacy QML FFT bridge

Default:

```text
ISCAN_FFT_QML_PUBLISH=0
```

For diagnostic comparison only it can be re-enabled:

```text
ISCAN_FFT_QML_PUBLISH=1
```

The production `SpectrumGLPlot.qml` no longer subscribes to
`Mainwindows::fftFrameUpdated`, so enabling this is intended only for legacy
external consumers or targeted diagnostics.

## Auto scale

R17 QML sampled and sorted FFT arrays every 500 ms. R18 computes the bounded
median and 99.5 percentile natively in `WebSocketClient` and exposes:

```text
wsClient.fftNoiseDb
wsClient.fftStrongDb
```

QML keeps the existing smoothing/range policy using those two scalar values.

## Waterfall history

The old QML Canvas self-copy:

```text
ctx.drawImage(waterfallCanvas, ... waterfallCanvas ...)
```

is removed. `FftDisplayItem` owns a native `QImage` history buffer. Each new
row is inserted at row 0 and existing rows are shifted down using `memmove`.
The QML layer never holds waterfall FFT rows.

## Diagnostics

New checkpoints:

```text
115 NATIVE_SPECTRUM_FRAME
116 NATIVE_WATERFALL_FRAME
117 NATIVE_SPECTRUM_PAINT
118 NATIVE_WATERFALL_PAINT
```

Startup log includes:

```text
[R18 FFT Runtime] ...
renderer=native-qquickpainteditem
spectrumDisplayBins=8192
waterfallDisplayBins=1280
```

`fftDiagnostics()` also reports display frame counters and native auto-scale
statistics.

## Files changed

- `FftDisplayItem.h` (new)
- `FftDisplayItem.cpp` (new)
- `websocketclient.h`
- `websocketclient.cpp`
- `SpectrumGLPlot.qml`
- `CrashDiagnostics.h`
- `CrashDiagnostics.cpp`
- `main.cpp`
- `iScanMR10.pro`

## Target build

```bash
qmake iScanMR10.pro -spec linux-jetson-orin-g++
make -j$(nproc)
```

R18 was statically audited in the development environment, but the exact
Jetson Qt5Orin toolchain is not installed there, so target compilation and
runtime soak testing remain required.
