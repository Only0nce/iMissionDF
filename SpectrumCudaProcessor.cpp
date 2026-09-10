#include "SpectrumCudaProcessor.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace {
int paletteIndex(float value, float minDb, float maxDb, int count)
{
    if (count <= 1)
        return 0;
    const float range = std::max(1.0e-9f, maxDb - minDb);
    float normalized = (value - minDb) / range;
    normalized = std::max(0.0f, std::min(1.0f, normalized));
    return std::max(0, std::min(count - 1,
        static_cast<int>(std::lround(normalized * static_cast<float>(count - 1)))));
}
}

SpectrumCudaProcessor::SpectrumCudaProcessor() = default;

SpectrumCudaProcessor::~SpectrumCudaProcessor()
{
    unloadCudaPlugin();
}

bool SpectrumCudaProcessor::initialize()
{
    if (m_initialized)
        return true;

    m_initialized = true;
    const QByteArray requested = qgetenv("ISCAN_SPECTRUM_CUDA").trimmed().toLower();
    const bool forceCpu = requested == "0" || requested == "off" || requested == "false"
                       || requested == "cpu";
    if (forceCpu) {
        m_lastError = QStringLiteral("CUDA disabled by ISCAN_SPECTRUM_CUDA");
        return true;
    }

    QStringList candidates;
    const QString explicitLib = QString::fromLocal8Bit(qgetenv("ISCAN_SPECTRUM_CUDA_LIB")).trimmed();
    if (!explicitLib.isEmpty())
        candidates << explicitLib;

    const QString appDir = QCoreApplication::applicationDirPath();
    candidates << QDir(appDir).filePath(QStringLiteral("libiscan_spectrum_cuda.so"));
    candidates << QDir(appDir).filePath(QStringLiteral("../lib/libiscan_spectrum_cuda.so"));
    candidates << QStringLiteral("/opt/iScanMR10/lib/libiscan_spectrum_cuda.so");
    candidates << QStringLiteral("/usr/local/lib/libiscan_spectrum_cuda.so");
    candidates.removeDuplicates();

    QStringList errors;
    for (const QString &candidate : candidates) {
        if (candidate.contains(QLatin1Char('/')) && !QFileInfo::exists(candidate))
            continue;
        if (tryLoadCudaPlugin(candidate))
            return true;
        if (!m_lastError.isEmpty())
            errors << candidate + QStringLiteral(": ") + m_lastError;
    }

    m_lastError = errors.isEmpty()
            ? QStringLiteral("CUDA plugin not found; using CPU worker fallback")
            : errors.join(QStringLiteral(" | "));
    return true; // CPU worker fallback is always valid.
}

bool SpectrumCudaProcessor::tryLoadCudaPlugin(const QString &path)
{
    unloadCudaPlugin();

    std::unique_ptr<QLibrary> library(new QLibrary(path));
    if (!library->load()) {
        m_lastError = library->errorString();
        return false;
    }

    CreateFn createFn = reinterpret_cast<CreateFn>(library->resolve("iscan_cuda_spectrum_create"));
    DestroyFn destroyFn = reinterpret_cast<DestroyFn>(library->resolve("iscan_cuda_spectrum_destroy"));
    ProcessRowFn processFn = reinterpret_cast<ProcessRowFn>(library->resolve("iscan_cuda_process_waterfall_row"));
    RecolorFn recolorFn = reinterpret_cast<RecolorFn>(library->resolve("iscan_cuda_recolor_history"));
    if (!createFn || !destroyFn || !processFn || !recolorFn) {
        m_lastError = QStringLiteral("CUDA plugin ABI incomplete");
        library->unload();
        return false;
    }

    char errorText[512] = {};
    void *handle = nullptr;
    const int rc = createFn(&handle, errorText, sizeof(errorText));
    if (rc != 0 || !handle) {
        m_lastError = QString::fromLocal8Bit(errorText).trimmed();
        if (m_lastError.isEmpty())
            m_lastError = QStringLiteral("CUDA plugin initialization failed rc=%1").arg(rc);
        library->unload();
        return false;
    }

    m_cudaLibrary = std::move(library);
    m_cudaHandle = handle;
    m_createFn = createFn;
    m_destroyFn = destroyFn;
    m_processRowFn = processFn;
    m_recolorFn = recolorFn;
    m_cudaAvailable = true;
    m_loadedLibrary = QFileInfo(path).absoluteFilePath();
    m_lastError = QStringLiteral("loaded %1").arg(m_loadedLibrary);
    return true;
}

void SpectrumCudaProcessor::unloadCudaPlugin()
{
    if (m_cudaHandle && m_destroyFn)
        m_destroyFn(m_cudaHandle);
    m_cudaHandle = nullptr;
    m_createFn = nullptr;
    m_destroyFn = nullptr;
    m_processRowFn = nullptr;
    m_recolorFn = nullptr;
    m_cudaAvailable = false;
    m_loadedLibrary.clear();
    if (m_cudaLibrary) {
        m_cudaLibrary->unload();
        m_cudaLibrary.reset();
    }
}

QString SpectrumCudaProcessor::backendName() const
{
    return m_cudaAvailable ? QStringLiteral("cuda-plugin") : QStringLiteral("cpu-worker");
}

void SpectrumCudaProcessor::disableCuda(const QString &reason)
{
    unloadCudaPlugin();
    m_lastError = reason;
}

bool SpectrumCudaProcessor::processWaterfallRow(const QVector<float> &input,
                                                int outputBins,
                                                float minDb,
                                                float maxDb,
                                                const QVector<quint32> &paletteLut,
                                                QVector<float> *outputDb,
                                                QVector<quint32> *outputArgb,
                                                bool *usedCuda)
{
    if (usedCuda)
        *usedCuda = false;
    if (!m_initialized)
        initialize();
    if (!outputDb || !outputArgb || input.isEmpty() || outputBins <= 0 || paletteLut.isEmpty())
        return false;

    outputBins = std::max(1, std::min(outputBins, input.size()));
    outputDb->resize(outputBins);
    outputArgb->resize(outputBins);

    if (m_cudaAvailable && m_cudaHandle && m_processRowFn) {
        char errorText[512] = {};
        const int rc = m_processRowFn(m_cudaHandle,
                                      input.constData(), input.size(), outputBins,
                                      minDb, maxDb,
                                      paletteLut.constData(), paletteLut.size(),
                                      outputDb->data(), outputArgb->data(),
                                      errorText, sizeof(errorText));
        if (rc == 0) {
            if (usedCuda)
                *usedCuda = true;
            return true;
        }

        QString error = QString::fromLocal8Bit(errorText).trimmed();
        if (error.isEmpty())
            error = QStringLiteral("CUDA waterfall plugin failed rc=%1").arg(rc);
        disableCuda(error);
        qWarning() << "[SPECTRUM-CUDA] runtime fallback to CPU:" << error;
    }

    return processWaterfallRowCpu(input, outputBins, minDb, maxDb,
                                  paletteLut, outputDb, outputArgb);
}

bool SpectrumCudaProcessor::recolorHistory(const QVector<float> &historyDb,
                                           float minDb,
                                           float maxDb,
                                           const QVector<quint32> &paletteLut,
                                           QVector<quint32> *outputArgb,
                                           bool *usedCuda)
{
    if (usedCuda)
        *usedCuda = false;
    if (!m_initialized)
        initialize();
    if (!outputArgb || historyDb.isEmpty() || paletteLut.isEmpty())
        return false;

    outputArgb->resize(historyDb.size());

    if (m_cudaAvailable && m_cudaHandle && m_recolorFn) {
        char errorText[512] = {};
        const int rc = m_recolorFn(m_cudaHandle,
                                   historyDb.constData(), historyDb.size(),
                                   minDb, maxDb,
                                   paletteLut.constData(), paletteLut.size(),
                                   outputArgb->data(),
                                   errorText, sizeof(errorText));
        if (rc == 0) {
            if (usedCuda)
                *usedCuda = true;
            return true;
        }

        QString error = QString::fromLocal8Bit(errorText).trimmed();
        if (error.isEmpty())
            error = QStringLiteral("CUDA recolor plugin failed rc=%1").arg(rc);
        disableCuda(error);
        qWarning() << "[SPECTRUM-CUDA] runtime fallback to CPU:" << error;
    }

    return recolorHistoryCpu(historyDb, minDb, maxDb, paletteLut, outputArgb);
}

bool SpectrumCudaProcessor::processWaterfallRowCpu(const QVector<float> &input,
                                                   int outputBins,
                                                   float minDb,
                                                   float maxDb,
                                                   const QVector<quint32> &paletteLut,
                                                   QVector<float> *outputDb,
                                                   QVector<quint32> *outputArgb) const
{
    const int paletteCount = paletteLut.size();
    for (int x = 0; x < outputBins; ++x) {
        const int sourceBegin = static_cast<int>((static_cast<qint64>(x) * input.size()) / outputBins);
        int sourceEnd = static_cast<int>((static_cast<qint64>(x + 1) * input.size()) / outputBins);
        sourceEnd = std::max(sourceBegin + 1, sourceEnd);
        sourceEnd = std::min(input.size(), sourceEnd);

        float peak = input.at(std::min(sourceBegin, input.size() - 1));
        if (!std::isfinite(peak))
            peak = minDb;
        for (int i = sourceBegin + 1; i < sourceEnd; ++i) {
            const float v = input.at(i);
            if (std::isfinite(v) && v > peak)
                peak = v;
        }

        (*outputDb)[x] = peak;
        const float clamped = std::max(minDb, std::min(maxDb, peak));
        (*outputArgb)[x] = paletteLut.at(paletteIndex(clamped, minDb, maxDb, paletteCount));
    }
    return true;
}

bool SpectrumCudaProcessor::recolorHistoryCpu(const QVector<float> &historyDb,
                                              float minDb,
                                              float maxDb,
                                              const QVector<quint32> &paletteLut,
                                              QVector<quint32> *outputArgb) const
{
    const int paletteCount = paletteLut.size();
    for (int i = 0; i < historyDb.size(); ++i) {
        float v = historyDb.at(i);
        if (!std::isfinite(v))
            v = minDb;
        v = std::max(minDb, std::min(maxDb, v));
        (*outputArgb)[i] = paletteLut.at(paletteIndex(v, minDb, maxDb, paletteCount));
    }
    return true;
}
