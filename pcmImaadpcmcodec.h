#ifndef PCMIMAADPCMCODEC_H
#define PCMIMAADPCMCODEC_H

// imaadpcmcodec.h
#pragma once

#include <QByteArray>
#include <QVector>
#include <QtGlobal>

class PCMImaAdpcmCodec {
public:
    PCMImaAdpcmCodec();
    void reset();
    QVector<qint16> decodeWithSync(const QByteArray &data);

private:
    int stepIndex;
    int predictor;
    int step;
    int synchronized;
    QByteArray syncWord;
    int syncCounter;
    int phase;
    quint8 syncBuffer[4];
    int syncBufferIndex;

    qint16 decodeNibble(quint8 nibble);

    static const int indexTable[16];
    static const int stepTable[89];
};


#endif // IMAADPCMCODEC_H
