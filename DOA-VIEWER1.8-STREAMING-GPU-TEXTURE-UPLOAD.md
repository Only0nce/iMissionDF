# DOA-VIEWER1.8 — Streaming GPU Texture Upload

## Purpose

This revision continues after DOA-VIEWER1.7-r5, where CH1 FFT toggle and waterfall stability were confirmed usable.

The previous waterfall path already used SceneGraph texture rendering and CUDA/plugin row processing, but every visible waterfall update still created a new `QSGTexture` and replaced the whole `QSGSimpleTextureNode`. That is stable, but it costs CPU/driver time and is not a good insertion point for CUDA/OpenGL zero-copy.

## Main change

`FftWaterfallTextureItem` now has a persistent streaming SceneGraph texture path:

```text
waterfall ARGB image
  -> persistent DoaWaterfallStreamingTexture
  -> glTexSubImage2D update when size is stable
  -> same QSGSimpleTextureNode reused
```

This reduces per-frame allocation churn compared with:

```text
waterfall ARGB image
  -> createTextureFromImage()
  -> delete old QSG node
  -> create new QSG node
```

## Runtime fallback

The old conservative upload path is still available:

```bash
export ISCAN_DOA_WATERFALL_UPLOAD=safe
```

Default:

```bash
export ISCAN_DOA_WATERFALL_UPLOAD=stream
```

## What this is / is not

This is not full CUDA/OpenGL PBO zero-copy yet. It is the safer intermediate step:

```text
CUDA/plugin row processing -> CPU ARGB row -> persistent GL texture upload
```

The next phase can replace the upload stage with PBO/CUDA interop while preserving the QML contract and the texture node lifecycle.

## Logs

Expected startup log:

```text
[DOA-VIEWER1.8-STREAMING-GPU-TEXTURE] ... streamTextureUpload=true cudaGlInterop=upload-stage
[DOA-WF-UPLOAD] mode="stream" streamTexture=true env=ISCAN_DOA_WATERFALL_UPLOAD
```

Expected periodic telemetry:

```text
[DOA-GPU-WATERFALL] ... uploadMode="stream" streamTextureUpload=true streamFrames=... recreateFrames=0 cudaInterop=upload-stage
```

If the driver has trouble with streaming upload, use `ISCAN_DOA_WATERFALL_UPLOAD=safe` and compare.
