# DOA-VIEWER1.9 — Final Profiling / Adaptive Quality

## Purpose

This revision closes the DoA Viewer performance optimization phase after the
GPU spectrum, GPU waterfall texture, CUDA waterfall pipeline, visibility fixes,
CH1 FFT toggle parity, and streaming texture upload work.

The goal is production stability on Jetson: keep the page responsive under real
load without changing the RF/DF protocol, CH1..CH6 source mapping, MUSIC/ESPRIT,
networking, or database behavior.

## Main changes

1. Adds a page-level adaptive quality governor in `DoaViewer/ViewerPage.qml`.
2. Keeps the default at a balanced quality level.
3. Reduces only display cadence when the page cannot meet the requested FPS.
4. Raises quality back after sustained recovery.
5. Applies one coherent quality level to:
   - the publish/render scheduler,
   - Spectrum target FPS,
   - Waterfall target FPS,
   - Polar/MUSIC paint FPS.
6. Adds production telemetry:
   - `[DOA-VIEWER-PERF] ... targetFps=... quality=... adaptive=...`
   - `[DOA-ADAPTIVE] quality=... reason=...`
7. Throttles `[QT5-SQUELCH-RX]` log spam while preserving every `onSQLChanged()`
   signal and UI/audio behavior.

## Quality levels

| Level | Name | Intended use |
|---:|---|---|
| 0 | high | Plenty of headroom, higher visual cadence |
| 1 | balanced | Default production setting |
| 2 | economy | Recovery mode when the page is below budget |

The governor only changes display rate. It does not change FFT source ownership,
receiver tuning, DF ADC channel mapping, RFSoC commands, or DoA computation.

## Runtime behavior

The governor compares measured visible frame cadence with the target cadence over
a 5-second window. If the page is below budget for consecutive windows, it steps
down one quality level. If it recovers for several windows, it steps back up.
This avoids rapid oscillation.

## Acceptance criteria

1. DoA Viewer opens without termination.
2. CH1 FFT toggle still works.
3. CH2..CH6 FFT toggle still works.
4. Waterfall remains visible and continuous.
5. `[DOA-VIEWER-PERF]` logs include `targetFps`, `quality`, and `adaptive`.
6. Under heavy load, `[DOA-ADAPTIVE]` may step down to balanced/economy.
7. After recovery, quality can step back up.
8. `[QT5-SQUELCH-RX]` no longer floods the log, but squelch UI behavior remains.
