# DOA-VIEWER1.6 — GPU Waterfall Texture

## Goal
Reduce CPU load in the DoA Viewer after DOA-VIEWER1.5 moved the spectrum trace to Qt SceneGraph/GPU. This revision targets the remaining Waterfall hot path.

## What changed
- Added `FftWaterfallTextureItem`, a native `QQuickItem` that renders the waterfall as a Qt SceneGraph texture.
- Replaced the native Waterfall path in `DoaViewer/WaterfallCanvas.qml` from `FftDisplayItem.Waterfall` / `QQuickPaintedItem` to `FftWaterfallTextureItem`.
- Kept the legacy QML Canvas fallback for compatibility when `nativeRenderEnabled` is false.
- Kept CH mapping unchanged:
  - CH1 = RX/Home spectrum + waterfall data
  - CH2..CH6 = DF CH1..CH5
- Kept MUSIC/ESPRIT/Polar/RFSoC protocol unchanged.

## Rendering model
Input waterfall row:

```text
QML selected FFT frame
    -> FftWaterfallTextureItem.submitExternalFrame()
    -> peak-preserving bucket pooling to item width
    -> colorized ARGB ring history
    -> Qt SceneGraph texture
    -> GPU compositing/scaling
```

This removes the native waterfall from the `QQuickPaintedItem`/`QPainter` path used by `FftDisplayItem`.

## Why this is phase 1.6 and not full interop yet
This revision still creates a Qt texture from an image-backed ring history. It avoids QML Canvas and QPainter painting, but it does not yet use CUDA/OpenGL direct row upload. That is intentionally reserved for DOA-VIEWER1.7.

## New telemetry
Look for:

```text
[DOA-VIEWER1.6-GPU-WATERFALL] sceneGraphTexture=1 qPainterWaterfall=0 ...
[DOA-GPU-WATERFALL] textureFps=... inputFps=... targetFps=... sceneGraph=1 qPainter=0 cudaInterop=0
```

`cudaInterop=0` is expected in this revision. In 1.7 it should become `1` if direct CUDA/OpenGL interop is active.

## Verification
Static verification script:

```bash
./VERIFY-DOA-VIEWER1.6-GPU-WATERFALL.sh
```

Expected:

```text
VERIFY: 19 PASS / 0 FAIL
```

## Runtime test
1. Clean build / rerun qmake because new C++ files and a new QML type were added.
2. Open DoA Viewer.
3. Test CH1 and CH2..CH6.
4. Watch CPU threads:

```bash
pidstat -t -p $(pidof iScanMR10) 1
# or
top -H -p $(pidof iScanMR10)
```

5. Confirm logs show `[DOA-GPU-WATERFALL]` and no QML Canvas fallback during normal native mode.

## Next phase
DOA-VIEWER1.7 should replace full texture upload with CUDA/OpenGL PBO or texture interop so CUDA can write the waterfall row directly into GPU-visible storage.
