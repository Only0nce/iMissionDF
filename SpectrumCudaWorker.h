#pragma once

#include "SpectrumCudaProcessor.h"

#include <QObject>
#include <QVector>

class SpectrumCudaWorker : public QObject
{
    Q_OBJECT
public:
    explicit SpectrumCudaWorker(QObject *parent = nullptr);

public slots:
    void initialize();
    void processWaterfallRow(quint64 generation,
                             QVector<float> frame,
                             int outputBins,
                             float minDb,
                             float maxDb,
                             QVector<quint32> paletteLut);
    void recolorHistory(quint64 generation,
                        QVector<float> historyDb,
                        float minDb,
                        float maxDb,
                        QVector<quint32> paletteLut);

signals:
    void backendReady(QString backendName, bool cudaActive, QString detail);
    void waterfallRowReady(quint64 generation,
                           QVector<float> dbRow,
                           QVector<quint32> argbRow,
                           bool usedCuda,
                           qint64 elapsedUsec);
    void historyRecolorReady(quint64 generation,
                             QVector<quint32> argbHistory,
                             bool usedCuda,
                             qint64 elapsedUsec);

private:
    SpectrumCudaProcessor m_processor;
};
