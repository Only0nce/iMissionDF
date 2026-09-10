#include "SpectrumCudaWorker.h"

#include <QElapsedTimer>
#include <QDebug>

SpectrumCudaWorker::SpectrumCudaWorker(QObject *parent)
    : QObject(parent)
{
}

void SpectrumCudaWorker::initialize()
{
    m_processor.initialize();
    emit backendReady(m_processor.backendName(),
                      m_processor.cudaAvailable(),
                      m_processor.lastError());
}

void SpectrumCudaWorker::processWaterfallRow(quint64 generation,
                                             QVector<float> frame,
                                             int outputBins,
                                             float minDb,
                                             float maxDb,
                                             QVector<quint32> paletteLut)
{
    QElapsedTimer timer;
    timer.start();

    QVector<float> dbRow;
    QVector<quint32> argbRow;
    bool usedCuda = false;
    m_processor.processWaterfallRow(frame, outputBins, minDb, maxDb,
                                    paletteLut, &dbRow, &argbRow, &usedCuda);

    emit waterfallRowReady(generation, dbRow, argbRow, usedCuda,
                           timer.nsecsElapsed() / 1000);
}

void SpectrumCudaWorker::recolorHistory(quint64 generation,
                                        QVector<float> historyDb,
                                        float minDb,
                                        float maxDb,
                                        QVector<quint32> paletteLut)
{
    QElapsedTimer timer;
    timer.start();

    QVector<quint32> colors;
    bool usedCuda = false;
    m_processor.recolorHistory(historyDb, minDb, maxDb,
                               paletteLut, &colors, &usedCuda);

    emit historyRecolorReady(generation, colors, usedCuda,
                             timer.nsecsElapsed() / 1000);
}
