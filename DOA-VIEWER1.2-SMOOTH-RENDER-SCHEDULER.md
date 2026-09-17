# DOA-VIEWER1.2 — Smooth Render Scheduler

## Goal
Reduce DoA Viewer stutter after adding logical CH1 RX + CH2..CH6 DF display sources, while preserving the current DoA Viewer layout and RF/DF behavior.

## Scope
Changed only the DoA Viewer presentation/performance path:

- `DoaViewer/ViewerPage.qml`
- `DoaViewer/FftPlot.qml`
- `DoaViewer/WaterfallCanvas.qml`
- `DoaViewer/DoaPolarPlot.qml`
- `Mainwindows.h`

No RFSoC protocol, MUSIC/ESPRIT computation, network endpoint, database, or CH1..CH6 mapping contract is changed.

## Main changes

### 1. One display scheduler for Spectrum + Waterfall
Incoming RX/DF FFT frames are now coalesced into a bounded display clock:

- RX/CH1: about 30 FPS display publishing
- DF/CH2..CH6: about 25 FPS display publishing

The UI always shows the newest available frame and does not replay stale backlog.

### 2. Latest-frame-only cache
`ViewerPage.qml` no longer binds `FftPlot` and `WaterfallCanvas` directly to high-rate backend FFT arrays. It caches the selected source into:

- `displayFftFreqHz`
- `displayFftMagDb`
- `displayFrameSequence`

This reduces QVariantList rebinding and Canvas invalidation during RF bursts.

### 3. Sequence-gated waterfall
`WaterfallCanvas.qml` now appends a row only when `frameSequence` changes. This fixes the old behavior where the waterfall could continue scrolling the same FFT row repeatedly based only on a Timer.

### 4. Waterfall row draw optimization
The waterfall row painter now caches RGB strings and groups adjacent equal-color pixels into runs. This reduces per-row Canvas `fillStyle`/`fillRect` calls on Jetson.

### 5. Polar paint throttle
`DoaPolarPlot.qml` now marks itself dirty on new data and paints on a bounded timer (`paintFps`, default 15 FPS). This prevents high-rate DoAResult updates from starving spectrum/waterfall drawing.

### 6. Removed stale direct DoaClient trigger in FftPlot
`FftPlot.qml` no longer repaints directly from global `doaClient.fftChanged`. It repaints only when its own `magDb/freqHz` inputs change through the selected display-source scheduler.

### 7. RX CH1 display budget
`Mainwindows.h` now limits the secondary DoA CH1 RX view to 768 display bins and 40 ms publish interval. Home/RX DSP remains untouched.

## Runtime telemetry
Every 5 seconds while DoA Viewer is visible:

```text
[DOA-VIEWER-PERF] source=RX frames5s=... fps=... bins=... seq=...
```

Use this to verify that the page is not trying to draw unbounded backend frame rate.

## Expected behavior

- CH1 still displays RX/Home spectrum and waterfall with DoA Viewer styling.
- CH2..CH6 still map to DF CH1..CH5.
- MUSIC/ESPRIT polar plot remains DF coherent 5-channel based.
- Waterfall no longer creates fake rows when no new visible FFT frame exists.
- Stutter should be reduced during fast channel switching and RF spectrum updates.

## Hardware validation checklist

1. Open DoA Viewer and keep CH1 selected for 10 minutes.
2. Confirm `[DOA-VIEWER-PERF] source=RX` stays around 20–30 FPS, not unbounded.
3. Switch CH1 → CH6 → CH2 → CH1 rapidly.
4. Confirm no stale axis/waterfall rows remain after switching source.
5. Confirm Polar remains active while switching spectrum source.
6. Confirm no `Audio queue overflow` log storm returns.
7. Confirm CPU/GPU load is lower or UI feels smoother than DOA-VIEWER1.1-r2.
