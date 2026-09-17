# DOA-VIEWER1.7-r3 — Waterfall Pipeline Diagnostic + Recovery

## Scope
This revision fixes the regression where the DoA Spectrum renders correctly but the native Waterfall remains visually empty after DOA-VIEWER1.7 / 1.7-r2.

## Root Cause Addressed
The Spectrum and Waterfall share the same `displayFftMagDb` source, so a live Spectrum proves the RF/FFT source is available. The blank Waterfall was caused by lifecycle and submission timing issues in the native Waterfall path:

1. `WaterfallCanvas.qml` could submit on `frameSequence` before the bound `waterfallRowDb` array had settled.
2. QML width/height changes reset native history too aggressively during layout/StackView sizing.
3. `FftWaterfallTextureItem::updatePaintNode()` returned `nullptr` while the history image was not yet created, so the SceneGraph node disappeared until a successful row arrived.
4. The CUDA/plugin row path is asynchronous; if the first completion is delayed or cancelled by early geometry changes, the panel can stay empty.

## Changes
- Deferred native row submission with `Qt.callLater()` so `waterfallRowDb` and `frameSequence` are aligned.
- Added `onWaterfallRowDbChanged` recovery submit path.
- Removed QML history clearing on native width/height jitter; C++ now owns native geometry recovery.
- C++ now creates a background SceneGraph texture as soon as valid geometry exists, even before the first row.
- Added first-visible CPU bootstrap row while CUDA/plugin warms up.
- Added defensive dB-row fallback if worker returns dB data without ARGB data.
- Hardened geometry changes to avoid null-node/destroy/recreate loops.
- Added telemetry fields: `bootstrapCpuRows`, `deferredSubmits`, and `[DOA-WF-FIRST-ROW]` / `[DOA-WF-GEOM]` logs.

## Expected Logs
```
[DOA-WF-FIRST-ROW] reason= cuda-bootstrap ... validRows= 1
[DOA-GPU-WATERFALL] ... validRows=... bootstrapCpuRows=... cudaRows=... cpuRows=...
```

## Test Modes
Existing runtime modes remain valid:

```
unset ISCAN_DOA_WATERFALL_BACKEND          # auto
export ISCAN_DOA_WATERFALL_BACKEND=cpu     # force CPU row processing
export ISCAN_DOA_WATERFALL_BACKEND=cuda    # prefer CUDA/plugin path
export ISCAN_DOA_WATERFALL_BACKEND=testpattern
```

## Acceptance
- DoA page no longer terminates.
- Spectrum still renders.
- Waterfall becomes visible on CH1 and CH2–CH6.
- MAP → DOA → MAP → DOA keeps Waterfall visible.
- Switching channels does not leave stale source rows.
- `testpattern` mode shows a visible Waterfall even without FFT/CUDA input.
