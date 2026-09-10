#pragma once

#include <QString>
#include <QVector>
#include <QtGlobal>

#include <memory>

class QLibrary;

class SpectrumCudaProcessor
{
public:
    SpectrumCudaProcessor();
    ~SpectrumCudaProcessor();

    bool initialize();
    bool cudaAvailable() const noexcept { return m_cudaAvailable; }
    QString backendName() const;
    QString lastError() const { return m_lastError; }
    QString loadedLibrary() const { return m_loadedLibrary; }

    bool processWaterfallRow(const QVector<float> &input,
                             int outputBins,
                             float minDb,
                             float maxDb,
                             const QVector<quint32> &paletteLut,
                             QVector<float> *outputDb,
                             QVector<quint32> *outputArgb,
                             bool *usedCuda = nullptr);

    bool recolorHistory(const QVector<float> &historyDb,
                        float minDb,
                        float maxDb,
                        const QVector<quint32> &paletteLut,
                        QVector<quint32> *outputArgb,
                        bool *usedCuda = nullptr);

private:
    using CreateFn = int (*)(void **, char *, int);
    using DestroyFn = void (*)(void *);
    using ProcessRowFn = int (*)(void *, const float *, int, int, float, float,
                                 const quint32 *, int, float *, quint32 *, char *, int);
    using RecolorFn = int (*)(void *, const float *, int, float, float,
                              const quint32 *, int, quint32 *, char *, int);

    bool tryLoadCudaPlugin(const QString &path);
    void unloadCudaPlugin();
    bool processWaterfallRowCpu(const QVector<float> &input,
                                int outputBins,
                                float minDb,
                                float maxDb,
                                const QVector<quint32> &paletteLut,
                                QVector<float> *outputDb,
                                QVector<quint32> *outputArgb) const;
    bool recolorHistoryCpu(const QVector<float> &historyDb,
                           float minDb,
                           float maxDb,
                           const QVector<quint32> &paletteLut,
                           QVector<quint32> *outputArgb) const;
    void disableCuda(const QString &reason);

    std::unique_ptr<QLibrary> m_cudaLibrary;
    void *m_cudaHandle = nullptr;
    CreateFn m_createFn = nullptr;
    DestroyFn m_destroyFn = nullptr;
    ProcessRowFn m_processRowFn = nullptr;
    RecolorFn m_recolorFn = nullptr;
    bool m_initialized = false;
    bool m_cudaAvailable = false;
    QString m_loadedLibrary;
    QString m_lastError;
};
