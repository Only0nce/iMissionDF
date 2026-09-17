# DOA-VIEWER1.4 — CPU Budget / Event-Driven Native Renderer

## Objective

Reduce the DoA Viewer CPU hot-core load seen in `jtop` after DOA-VIEWER1.3 moved the trace/waterfall drawing to the native/CUDA renderer. The prior version was visually faster, but one CPU core could remain near 97–100% because the presentation path still repainted native analyzer items continuously.

## Root Cause Addressed

DOA-VIEWER1.3 still had a permanent shared analyzer presentation clock. Each visible `FftDisplayItem` repainted on every shared clock tick as long as it had data. In the DoA page this meant the native Spectrum and Waterfall items could continue issuing Qt Quick updates even when no new frame had arrived.

A second source of waste existed in `FftPlot.qml`: the native Spectrum item could be fed both by `onFrameSequenceChanged` and by the local paint timer after the same frame marked the plot dirty.

## Changes

### FftDisplayItem

- Added per-item `targetFps` property.
- Added event-driven dirty presentation gate:
  - incoming Spectrum frame marks `m_presentDirty`;
  - completed Waterfall/CUDA row marks `m_presentDirty`;
  - shared clock only calls `update()` when dirty and the per-item FPS budget allows it.
- Reduced default shared analyzer clock from 90 FPS to 60 FPS using the same `ISCAN_ANALYZER_PRESENT_FPS` environment override.
- Added CPU-budget telemetry:
  - `[DOA-CPU-BUDGET] mode=... paintFps=... targetFps=... updates=... skipClean=... skipBudget=... externalFrames=...`
- Preserved CUDA worker path and native QPainter/FBO rendering.

### FftPlot.qml

- Native frame submission is now sequence-gated with `_lastNativeSubmittedSeq`.
- The paint timer may recover from QML binding-order edge cases, but duplicate submissions for the same `frameSequence` are skipped.
- Native Spectrum `targetFps` follows `root.fftFps`.

### WaterfallCanvas.qml

- Native Waterfall `targetFps` follows `root.wfFps`.
- Existing sequence-gated waterfall row behavior remains unchanged.

## Preserved Contracts

- CH1 remains AstraRX/Home RX Spectrum + Waterfall.
- CH2–CH6 remain DF CH1–CH5.
- MUSIC/ESPRIT/Polar path is unchanged.
- RFSoC command protocol is unchanged.
- `DoaClient.cpp/.h` are unchanged.
- CUDA plugin/fallback ABI is unchanged.

## Test Expectations

On target hardware, use:

```bash
pidstat -t -p $(pidof iScanMR10) 1
# or
top -H -p $(pidof iScanMR10)
```

Expected behavior:

- The previous always-hot render core should drop significantly.
- `[DOA-CPU-BUDGET]` should show `paintFps` close to the per-item `targetFps`, not the global clock rate.
- `skipClean` should be non-zero when no new data needs repaint.
- `externalFrames` should roughly match selected display publish rate.
- Waterfall CUDA telemetry remains available via `[SPECTRUM-COMPUTE-10S]`.

## Validation Performed Here

- Static verification: `VERIFY-DOA-VIEWER1.4-CPU-BUDGET.sh` passed.
- No target Qt 5.15.2 cross-build was available in this environment.
