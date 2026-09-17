# DOA-VIEWER1.7-r4 — Waterfall Width Stability / Stutter Fix

## Problem
After 1.7-r3 the DoA page no longer terminated, and Waterfall became partially visible, but it stuttered or appeared intermittently.

Runtime logs showed:

- `FftWaterfallTextureItem` geometry stabilized at `1228x238`.
- `DOA-WF-FIRST-ROW` repeated continuously instead of appearing once.
- `DOA-GPU-WATERFALL` reported `validRows=0` even though `cudaRows`, `cpuRows`, and `bootstrapCpuRows` were increasing.

This means data was entering the Waterfall pipeline, but the ring history was repeatedly reset before it could accumulate rows.

## Root Cause
The async CUDA/plugin worker can return an ARGB row whose width matches the worker/input row size, such as 768 or 1024 bins, while the actual SceneGraph texture width is the item width, such as 1228 pixels.

The previous `onWaterfallRowReady()` appended CUDA ARGB rows using:

```cpp
appendArgbRowLocked(argbRow, argbRow.size());
```

That caused the history texture width to oscillate:

```text
submit frame width = 1228
CUDA/plugin ARGB row width = 768 or 1024
append result width = 768/1024
next submit width = 1228
ensureHistoryLocked() sees a different width and resets validRows to 0
```

So every new frame looked like a first row, producing repeated `cuda-bootstrap`, `validRows=1`, then reset back to `validRows=0`.

## Fix
1. Track the intended output width for the in-flight CUDA job with `m_inFlightOutputWidth`.
2. Append completed CUDA/CPU worker rows using the original intended texture width, not `argbRow.size()`.
3. Let `appendArgbRowLocked()` resample the returned ARGB row to the stable texture width.
4. Reset `m_inFlightOutputWidth` safely when render is disabled, geometry changes, CUDA is disabled, history is cleared, or the item is destroyed.
5. Add telemetry:

```text
widthNormalizedRows=...
argbInW=...
desiredW=...
```

Expected behavior after the fix:

```text
validRows increases toward texture height
DOA-WF-FIRST-ROW appears only during startup/clear/source change
widthNormalizedRows may be >0 when plugin output width differs from item width
Waterfall scrolls continuously instead of appearing intermittently
```

## Scope
Changed files:

- `FftWaterfallTextureItem.h`
- `FftWaterfallTextureItem.cpp`

No RFSoC protocol, DoA algorithm, Spectrum renderer, network, database, or CH1–CH6 mapping changes.
