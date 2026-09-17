# DOA-VIEWER1.5 — SceneGraph GPU Spectrum Renderer

## Goal
Reduce the remaining high CPU load on the DoA Viewer by moving the live Spectrum trace/fill away from `QQuickPaintedItem`/`QPainter` and onto Qt SceneGraph geometry. This is the next step after DOA-VIEWER1.4, which introduced CPU budgeting and event-driven repaint gates.

## Scope
Changed only the DoA Viewer spectrum rendering path. The following contracts are preserved:

- CH1 remains RX/Home Spectrum + Waterfall data rendered in DoA Viewer style.
- CH2..CH6 remain DF CH1..CH5 logical mapping.
- Waterfall continues to use the existing `FftDisplayItem` native/CUDA worker path.
- MUSIC / ESPRIT / Polar are unchanged.
- RFSoC protocol, Network Settings, DB endpoint logic and DoA command contract are unchanged.

## Main Change
Added `FftLineGraphItem`, a lightweight `QQuickItem` that renders the live FFT spectrum using Qt SceneGraph:

```text
FFT magnitudes
  -> FftLineGraphItem::submitExternalFrame()
  -> peak-preserving per-pixel bucket pooling
  -> QSGGeometry triangle-strip fill
  -> QSGGeometry line-strip trace
  -> GPU SceneGraph render
```

This replaces the previous DoA spectrum native path:

```text
FFT magnitudes
  -> FftDisplayItem
  -> QQuickPaintedItem
  -> QPainter polygon/line
  -> texture upload/composite
```

## Files Changed

- `FftLineGraphItem.h` — new SceneGraph spectrum item.
- `FftLineGraphItem.cpp` — new GPU geometry renderer and telemetry.
- `main.cpp` — registers `FftLineGraphItem` as `iScan.Display 1.0`.
- `iScanMR10.pro` — adds new source/header to x86 and Jetson file lists.
- `DoaViewer/FftPlot.qml` — uses `FftLineGraphItem` for `nativeSpectrumItem`.
- `VERIFY-DOA-VIEWER1.5-GPU-SPECTRUM.sh` — structural regression checks.

## Performance Design

### Peak-preserving pooling
The renderer does not skip bins blindly. For each output x bucket, it keeps the strongest FFT bin. This keeps narrow carriers visible while limiting geometry to roughly one point per display pixel.

### Coalesced updates
`submitExternalFrame()` always stores the newest frame. `update()` is budgeted by `targetFps`, and repeated frames within the same budget window are coalesced.

### GPU geometry
The live fill is a `DrawTriangleStrip`; the trace is a `DrawLineStrip`. QML still draws grid, text, marker and target-band overlays.

## New Telemetry
Every ~5 seconds the renderer logs:

```text
[DOA-GPU-SPECTRUM] paintFps=... inputFps=... targetFps=... bins=... points=... skipBudget=... skipPending=... sceneGraph=1 qPainter=0
```

Expected behavior:

- `paintFps` should stay near `targetFps`.
- `points` should be close to the visible plot width, not the raw FFT bin count.
- `skipBudget` and/or `skipPending` may increase under load; that means the newest-frame coalescing path is working.

## Test Plan

1. Clean build so moc/qmake sees the new `FftLineGraphItem` type.
2. Open DoA Viewer on CH1 and watch `jtop` / `top -H`.
3. Check `[DOA-GPU-SPECTRUM]` logs.
4. Switch CH1 -> CH6 -> CH2 -> CH1 quickly.
5. Confirm Spectrum, Waterfall and Polar still update.
6. Compare CPU against DOA-VIEWER1.4.

## Known Remaining Work
This revision moves Spectrum trace/fill to SceneGraph GPU geometry. Waterfall still uses `FftDisplayItem`/CUDA worker and QPainter texture presentation. If one CPU core is still high, the next planned step is DOA-VIEWER1.6: GPU texture waterfall / SceneGraph waterfall surface.
