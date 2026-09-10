#include <cuda_runtime.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>

namespace {
struct SpectrumCudaContext {
    cudaStream_t stream = nullptr;
    float *dInput = nullptr;
    int inputCapacity = 0;
    float *dOutputDb = nullptr;
    int outputDbCapacity = 0;
    std::uint32_t *dOutputArgb = nullptr;
    int outputArgbCapacity = 0;
    float *dHistoryDb = nullptr;
    int historyDbCapacity = 0;
    std::uint32_t *dHistoryArgb = nullptr;
    int historyArgbCapacity = 0;
    std::uint32_t *dPalette = nullptr;
    int paletteCapacity = 0;
};

void setError(char *dst, int bytes, const char *prefix, cudaError_t err)
{
    if (!dst || bytes <= 0)
        return;
    std::snprintf(dst, static_cast<std::size_t>(bytes), "%s: %s", prefix, cudaGetErrorString(err));
}

bool ensureFloat(float **ptr, int *capacity, int required, char *errorText, int errorBytes)
{
    if (required <= *capacity)
        return true;
    if (*ptr)
        cudaFree(*ptr);
    *ptr = nullptr;
    *capacity = 0;
    cudaError_t err = cudaMalloc(reinterpret_cast<void **>(ptr), sizeof(float) * static_cast<std::size_t>(required));
    if (err != cudaSuccess) {
        setError(errorText, errorBytes, "cudaMalloc(float)", err);
        return false;
    }
    *capacity = required;
    return true;
}

bool ensureU32(std::uint32_t **ptr, int *capacity, int required, char *errorText, int errorBytes)
{
    if (required <= *capacity)
        return true;
    if (*ptr)
        cudaFree(*ptr);
    *ptr = nullptr;
    *capacity = 0;
    cudaError_t err = cudaMalloc(reinterpret_cast<void **>(ptr), sizeof(std::uint32_t) * static_cast<std::size_t>(required));
    if (err != cudaSuccess) {
        setError(errorText, errorBytes, "cudaMalloc(uint32)", err);
        return false;
    }
    *capacity = required;
    return true;
}

__device__ __forceinline__ int clampPaletteIndex(float value, float minDb, float maxDb, int paletteCount)
{
    const float range = fmaxf(1.0e-9f, maxDb - minDb);
    float normalized = (value - minDb) / range;
    normalized = fminf(1.0f, fmaxf(0.0f, normalized));
    int index = static_cast<int>(floorf(normalized * static_cast<float>(paletteCount - 1) + 0.5f));
    return max(0, min(paletteCount - 1, index));
}

__global__ void waterfallRowKernel(const float *input,
                                   int inputCount,
                                   int outputBins,
                                   float minDb,
                                   float maxDb,
                                   const std::uint32_t *palette,
                                   int paletteCount,
                                   float *outputDb,
                                   std::uint32_t *outputArgb)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    if (x >= outputBins)
        return;

    int begin = static_cast<int>((static_cast<long long>(x) * inputCount) / outputBins);
    int end = static_cast<int>((static_cast<long long>(x + 1) * inputCount) / outputBins);
    end = max(begin + 1, end);
    end = min(inputCount, end);

    float peak = input[min(begin, inputCount - 1)];
    if (!isfinite(peak))
        peak = minDb;
    for (int i = begin + 1; i < end; ++i) {
        const float v = input[i];
        if (isfinite(v) && v > peak)
            peak = v;
    }

    outputDb[x] = peak;
    float clamped = fminf(maxDb, fmaxf(minDb, peak));
    outputArgb[x] = palette[clampPaletteIndex(clamped, minDb, maxDb, paletteCount)];
}

__global__ void recolorKernel(const float *historyDb,
                              int count,
                              float minDb,
                              float maxDb,
                              const std::uint32_t *palette,
                              int paletteCount,
                              std::uint32_t *outputArgb)
{
    const int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= count)
        return;
    float value = historyDb[i];
    if (!isfinite(value))
        value = minDb;
    value = fminf(maxDb, fmaxf(minDb, value));
    outputArgb[i] = palette[clampPaletteIndex(value, minDb, maxDb, paletteCount)];
}
}

extern "C" int iscan_cuda_spectrum_create(void **handle, char *errorText, int errorTextBytes)
{
    if (!handle)
        return -1;
    *handle = nullptr;

    int deviceCount = 0;
    cudaError_t err = cudaGetDeviceCount(&deviceCount);
    if (err != cudaSuccess) {
        setError(errorText, errorTextBytes, "cudaGetDeviceCount", err);
        return -2;
    }
    if (deviceCount <= 0) {
        if (errorText && errorTextBytes > 0)
            std::snprintf(errorText, static_cast<std::size_t>(errorTextBytes), "no CUDA device");
        return -3;
    }

    SpectrumCudaContext *ctx = new SpectrumCudaContext();
    err = cudaStreamCreateWithFlags(&ctx->stream, cudaStreamNonBlocking);
    if (err != cudaSuccess) {
        setError(errorText, errorTextBytes, "cudaStreamCreateWithFlags", err);
        delete ctx;
        return -4;
    }

    *handle = ctx;
    return 0;
}

extern "C" void iscan_cuda_spectrum_destroy(void *handle)
{
    SpectrumCudaContext *ctx = static_cast<SpectrumCudaContext *>(handle);
    if (!ctx)
        return;
    if (ctx->stream)
        cudaStreamSynchronize(ctx->stream);
    if (ctx->dInput) cudaFree(ctx->dInput);
    if (ctx->dOutputDb) cudaFree(ctx->dOutputDb);
    if (ctx->dOutputArgb) cudaFree(ctx->dOutputArgb);
    if (ctx->dHistoryDb) cudaFree(ctx->dHistoryDb);
    if (ctx->dHistoryArgb) cudaFree(ctx->dHistoryArgb);
    if (ctx->dPalette) cudaFree(ctx->dPalette);
    if (ctx->stream) cudaStreamDestroy(ctx->stream);
    delete ctx;
}

extern "C" int iscan_cuda_process_waterfall_row(void *handle,
                                                 const float *input,
                                                 int inputCount,
                                                 int outputBins,
                                                 float minDb,
                                                 float maxDb,
                                                 const std::uint32_t *palette,
                                                 int paletteCount,
                                                 float *outputDb,
                                                 std::uint32_t *outputArgb,
                                                 char *errorText,
                                                 int errorTextBytes)
{
    SpectrumCudaContext *ctx = static_cast<SpectrumCudaContext *>(handle);
    if (!ctx || !input || inputCount <= 0 || outputBins <= 0 || !palette || paletteCount <= 0
            || !outputDb || !outputArgb)
        return -1;

    if (!ensureFloat(&ctx->dInput, &ctx->inputCapacity, inputCount, errorText, errorTextBytes)
            || !ensureFloat(&ctx->dOutputDb, &ctx->outputDbCapacity, outputBins, errorText, errorTextBytes)
            || !ensureU32(&ctx->dOutputArgb, &ctx->outputArgbCapacity, outputBins, errorText, errorTextBytes)
            || !ensureU32(&ctx->dPalette, &ctx->paletteCapacity, paletteCount, errorText, errorTextBytes))
        return -2;

    cudaError_t err = cudaMemcpyAsync(ctx->dInput, input,
                                      sizeof(float) * static_cast<std::size_t>(inputCount),
                                      cudaMemcpyHostToDevice, ctx->stream);
    if (err != cudaSuccess) { setError(errorText, errorTextBytes, "copy input H2D", err); return -3; }
    err = cudaMemcpyAsync(ctx->dPalette, palette,
                          sizeof(std::uint32_t) * static_cast<std::size_t>(paletteCount),
                          cudaMemcpyHostToDevice, ctx->stream);
    if (err != cudaSuccess) { setError(errorText, errorTextBytes, "copy palette H2D", err); return -4; }

    const int block = 256;
    const int grid = (outputBins + block - 1) / block;
    waterfallRowKernel<<<grid, block, 0, ctx->stream>>>(ctx->dInput, inputCount, outputBins,
                                                        minDb, maxDb, ctx->dPalette, paletteCount,
                                                        ctx->dOutputDb, ctx->dOutputArgb);
    err = cudaGetLastError();
    if (err != cudaSuccess) { setError(errorText, errorTextBytes, "waterfallRowKernel", err); return -5; }

    err = cudaMemcpyAsync(outputDb, ctx->dOutputDb,
                          sizeof(float) * static_cast<std::size_t>(outputBins),
                          cudaMemcpyDeviceToHost, ctx->stream);
    if (err != cudaSuccess) { setError(errorText, errorTextBytes, "copy db D2H", err); return -6; }
    err = cudaMemcpyAsync(outputArgb, ctx->dOutputArgb,
                          sizeof(std::uint32_t) * static_cast<std::size_t>(outputBins),
                          cudaMemcpyDeviceToHost, ctx->stream);
    if (err != cudaSuccess) { setError(errorText, errorTextBytes, "copy color D2H", err); return -7; }

    err = cudaStreamSynchronize(ctx->stream);
    if (err != cudaSuccess) { setError(errorText, errorTextBytes, "waterfall stream sync", err); return -8; }
    return 0;
}

extern "C" int iscan_cuda_recolor_history(void *handle,
                                           const float *historyDb,
                                           int valueCount,
                                           float minDb,
                                           float maxDb,
                                           const std::uint32_t *palette,
                                           int paletteCount,
                                           std::uint32_t *outputArgb,
                                           char *errorText,
                                           int errorTextBytes)
{
    SpectrumCudaContext *ctx = static_cast<SpectrumCudaContext *>(handle);
    if (!ctx || !historyDb || valueCount <= 0 || !palette || paletteCount <= 0 || !outputArgb)
        return -1;

    if (!ensureFloat(&ctx->dHistoryDb, &ctx->historyDbCapacity, valueCount, errorText, errorTextBytes)
            || !ensureU32(&ctx->dHistoryArgb, &ctx->historyArgbCapacity, valueCount, errorText, errorTextBytes)
            || !ensureU32(&ctx->dPalette, &ctx->paletteCapacity, paletteCount, errorText, errorTextBytes))
        return -2;

    cudaError_t err = cudaMemcpyAsync(ctx->dHistoryDb, historyDb,
                                      sizeof(float) * static_cast<std::size_t>(valueCount),
                                      cudaMemcpyHostToDevice, ctx->stream);
    if (err != cudaSuccess) { setError(errorText, errorTextBytes, "copy history H2D", err); return -3; }
    err = cudaMemcpyAsync(ctx->dPalette, palette,
                          sizeof(std::uint32_t) * static_cast<std::size_t>(paletteCount),
                          cudaMemcpyHostToDevice, ctx->stream);
    if (err != cudaSuccess) { setError(errorText, errorTextBytes, "copy palette H2D", err); return -4; }

    const int block = 256;
    const int grid = (valueCount + block - 1) / block;
    recolorKernel<<<grid, block, 0, ctx->stream>>>(ctx->dHistoryDb, valueCount,
                                                   minDb, maxDb, ctx->dPalette, paletteCount,
                                                   ctx->dHistoryArgb);
    err = cudaGetLastError();
    if (err != cudaSuccess) { setError(errorText, errorTextBytes, "recolorKernel", err); return -5; }

    err = cudaMemcpyAsync(outputArgb, ctx->dHistoryArgb,
                          sizeof(std::uint32_t) * static_cast<std::size_t>(valueCount),
                          cudaMemcpyDeviceToHost, ctx->stream);
    if (err != cudaSuccess) { setError(errorText, errorTextBytes, "copy recolor D2H", err); return -6; }
    err = cudaStreamSynchronize(ctx->stream);
    if (err != cudaSuccess) { setError(errorText, errorTextBytes, "recolor stream sync", err); return -7; }
    return 0;
}
