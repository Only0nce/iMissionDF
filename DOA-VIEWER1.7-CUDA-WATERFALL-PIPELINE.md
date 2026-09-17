# DOA-VIEWER1.7 — CUDA Waterfall Pipeline

## Goal

Continue after DOA-VIEWER1.6-r2, which fixed the StackView/SceneGraph termination. This revision keeps the stable SceneGraph texture renderer but moves the expensive waterfall row processing path toward GPU/CUDA execution.

## Problem addressed

DOA-VIEWER1.6 rendered the waterfall through `FftWaterfallTextureItem` and avoided QML Canvas/QPainter. However, the new texture item still did per-row work locally:

- QVariantList to `QVector<float>` conversion
- peak-preserving bucket pooling
- dB-to-palette colorization
- ring-history append
- full texture presentation through Qt SceneGraph

On Jetson, this can still leave one CPU core busy when the DoA page is open.

## Change summary

`FftWaterfallTextureItem` now uses the existing runtime CUDA plugin path through `SpectrumCudaWorker`:

```text
FFT row from QML
  -> FftWaterfallTextureItem
  -> latest-frame-only CUDA queue
  -> SpectrumCudaWorker / SpectrumCudaProcessor
  -> libiscan_spectrum_cuda.so when available
  -> ARGB row returned
  -> SceneGraph waterfall texture ring
```

No CUDA headers or CUDA libraries are linked into the main Qt application. The existing runtime plugin remains optional:

- CUDA plugin present: row peak pooling and colorization use CUDA.
- CUDA plugin missing/disabled: worker falls back to CPU safely.

## Stability constraints preserved

- `FftWaterfallTextureItem` keeps the 1.6-r2 conservative node replacement policy.
- No manual texture reuse/double-delete was reintroduced.
- `releaseResources()` cancels pending rows and delayed update requests.
- StackView root anchor fix in `ViewerPage.qml` remains unchanged.
- QML contract is unchanged except for optional `cudaWaterfallEnabled`.

## Latest-frame-only policy

If a row is in flight on the worker and newer FFT rows arrive, the item keeps only the latest pending row. This avoids replaying stale backlog:

```text
in-flight row 101
new rows 102, 103, 104 arrive
only row 104 is kept pending
when row 101 returns, stale display is dropped and row 104 is dispatched
```

This is correct for a live analyzer. Waterfall should stay close to the live edge rather than consume old frames.

## New telemetry

Startup:

```text
[DOA-VIEWER1.7-CUDA-WATERFALL] sceneGraphTexture=1 qPainterWaterfall=0 textureRingHistory=1 eventDriven=1 cudaPluginPipeline=1 cudaGlInterop=staged ...
```

Worker backend:

```text
[DOA-CUDA-WATERFALL] backend="cuda-plugin" cudaActive=true detail="..."
```

5-second render telemetry:

```text
[DOA-GPU-WATERFALL] textureFps=... inputFps=... backend="cuda-plugin" cudaActive=true cudaRows=... cpuRows=... dispatches=... coalesced=... staleRows=... lastCudaUsec=... cudaInterop="plugin-stage"
```

`cudaInterop=plugin-stage` means CUDA is now in the DoA waterfall row pipeline, but the final Qt texture upload is still SceneGraph-owned. True zero-copy CUDA/OpenGL PBO or texture interop is intentionally deferred because it requires a stricter OpenGL context ownership contract on Qt 5.15.

## Files changed

- `FftWaterfallTextureItem.h`
- `FftWaterfallTextureItem.cpp`
- `DoaViewer/WaterfallCanvas.qml`
- `DOA-VIEWER1.7-CUDA-WATERFALL-PIPELINE.md`
- `VERIFY-DOA-VIEWER1.7-CUDA-WATERFALL-PIPELINE.sh`

## Test focus

1. Build with clean qmake/make.
2. Open DoA Viewer CH1 for at least 10 minutes.
3. Confirm no terminate/regression from 1.6-r2.
4. Confirm telemetry shows `backend="cuda-plugin" cudaActive=true` when plugin is installed.
5. Confirm `cudaRows` increases and `cpuRows` remains low/zero when CUDA is available.
6. Watch `top -H -p $(pidof iScanMR10)` and compare one-core CPU load against 1.6-r2.

