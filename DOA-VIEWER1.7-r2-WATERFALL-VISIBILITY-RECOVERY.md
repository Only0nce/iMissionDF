# DOA-VIEWER1.7-r2 — Waterfall Visibility Recovery

## Purpose

This revision fixes the DOA-VIEWER1.7 regression where the DoA page no longer terminates, but the Waterfall display can disappear after moving the row pipeline to the CUDA worker.

## Root cause

The first 1.7 CUDA waterfall queue used a strict live-edge stale-row guard:

```text
if newer frame arrived while current row was in flight:
    drop completed row
```

When FFT input FPS is higher than the CUDA/CPU row completion FPS, there is almost always a newer pending frame by the time a row returns. That means every completed row can be dropped and the SceneGraph texture never receives visible data.

## Fix

1. Accept every completed row that belongs to the active generation.
2. Keep latest-frame-only behavior by dispatching the freshest pending row immediately after appending the completed row.
3. Drop rows only when they belong to a canceled generation or the renderer is disabled/released.
4. Add a selectable backend mode for debug and guaranteed visibility recovery:

```bash
export ISCAN_DOA_WATERFALL_BACKEND=auto        # default: CUDA plugin when available, CPU worker fallback
export ISCAN_DOA_WATERFALL_BACKEND=cuda        # prefer CUDA plugin, with existing processor fallback if plugin fails
export ISCAN_DOA_WATERFALL_BACKEND=cpu         # force local CPU row processing in FftWaterfallTextureItem
export ISCAN_DOA_WATERFALL_BACKEND=testpattern # render synthetic moving pattern to test texture/SceneGraph path
```

5. Add telemetry fields to identify exactly where rows disappear:

```text
[DOA-WF-BACKEND] mode=...
[DOA-GPU-WATERFALL] ... cudaRows=... cpuRows=... staleRows=... testPatternRows=... emptyRows=...
```

## Expected behavior

- Normal `auto` mode should show Waterfall again.
- `cpu` mode should always show Waterfall when FFT rows are arriving.
- `testpattern` mode should show a moving color pattern; if it does not, the issue is texture/SceneGraph/QML layout rather than CUDA/data.
- StackView/SceneGraph termination fixes from 1.6-r2 remain preserved.

## Test procedure

```bash
# 1) default/auto
unset ISCAN_DOA_WATERFALL_BACKEND
./iScanMR10

# 2) force CPU fallback
export ISCAN_DOA_WATERFALL_BACKEND=cpu
./iScanMR10

# 3) texture/SceneGraph test pattern
export ISCAN_DOA_WATERFALL_BACKEND=testpattern
./iScanMR10
```

Acceptance:

```text
1. DOA page does not terminate.
2. Waterfall is visible in CH1 and CH2-CH6.
3. MAP -> DOA -> MAP -> DOA keeps Waterfall visible.
4. Switching CH1-CH6 clears old history and shows new rows.
5. Telemetry shows rows entering one of cudaRows/cpuRows/testPatternRows.
```
