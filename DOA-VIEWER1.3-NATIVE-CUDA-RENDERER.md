# DOA-VIEWER1.3 — Native/CUDA DoA Renderer

Goal: reduce DoA Viewer stutter by removing high-rate JavaScript Canvas loops from the Spectrum/Waterfall hot path.

## What changed

- DoA Viewer still keeps the same layout and interaction model.
- CH1 remains AstraRX/Home RX data rendered in DoA Viewer style.
- CH2..CH6 remain DF CH1..CH5 logical mapping.
- MUSIC/ESPRIT/Polar and RFSoC protocol are unchanged.
- FftDisplayItem now exposes `submitExternalFrame(QVariantList)` so DoA Viewer can feed its selected FFT frame into the existing native renderer.
- FftPlot now uses a native `FftDisplayItem` trace when `nativeRenderEnabled` is true. The QML Canvas remains for low-rate grid/labels/target overlay only.
- WaterfallCanvas now uses a native `FftDisplayItem` waterfall when `nativeRenderEnabled` is true. The existing CPU/CUDA worker path handles row peak-pooling and colorization. QML Canvas fallback remains available.
- ViewerPage enables native rendering for both Spectrum and Waterfall.

## Important scope note

This revision uses the existing R20.4 Spectrum CUDA plugin path. CUDA accelerates the waterfall row processing/colorization and history recolor path when `libiscan_spectrum_cuda.so` is present. Spectrum trace painting is moved out of JavaScript Canvas into native Qt Quick/QPainter, which removes the heavy JS loop but is not CUDA/OpenGL interop yet.

Expected runtime when CUDA plugin is installed:

```text
[SPECTRUM-CUDA] backend= "cuda-plugin" cudaActive= true ...
[SPECTRUM-COMPUTE-10S] backend= "cuda-plugin" cudaRows= ... cpuRows= 0 ...
```

If the plugin is missing, the renderer falls back to the existing CPU worker safely.

## Files changed

```text
FftDisplayItem.h
FftDisplayItem.cpp
DoaViewer/FftPlot.qml
DoaViewer/WaterfallCanvas.qml
DoaViewer/ViewerPage.qml
```

## Acceptance

- Build succeeds on Qt 5.15.2 target.
- DoA page remains visually same style.
- CH1 shows RX/Home spectrum/waterfall in DoA Viewer layout.
- CH2..CH6 show DF CH1..CH5.
- Waterfall does not use per-pixel QML row drawing when native path is enabled.
- QML Canvas FFT trace loop is bypassed when native path is enabled.
- CUDA plugin activation can be verified in logs.
