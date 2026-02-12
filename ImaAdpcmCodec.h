#ifndef IMAADPCMCODEC_H
#define IMAADPCMCODEC_H

#pragma once

#include <QByteArray>
#include <QVector>
#include <algorithm>
#include <cstdint>

class ImaAdpcmCodec
{
public:
    ImaAdpcmCodec() { reset(); }

    void reset();

    QVector<qint16> decode(const QByteArray &data);
    void applyVolumeToPcm16(QVector<qint16> &samples, int volumePercent);

private:
    int stepIndex = 0;
    int predictor = 0;
    int step = 0;

    qint16 decodeNibble(quint8 nibble);

    static const int indexTable[16];
    static const int stepTable[89];
};


#endif // IMAADPCMCODEC_H
