#ifndef IMAADPCMCODEC_H
#define IMAADPCMCODEC_H

#pragma once

#include <QByteArray>
#include <QVector>
#include <QVariantList>
#include <algorithm>
#include <cstdint>

class ImaAdpcmCodec
{
public:
    ImaAdpcmCodec() { reset(); }

    void reset();

    QVector<qint16> decode(const QByteArray &data);

    // Decode the complete ADPCM frame while writing the scaled FFT values
    // directly into the QVariantList consumed by QML. This preserves the
    // original full-span FFT semantics while avoiding two intermediate
    // full-frame vectors (qint16 -> float).
    QVariantList decodeScaledToVariantList(const QByteArray &data,
                                           int skipSamples,
                                           float scale);

    // Decode a complete scaled frame into a reusable native vector. This is
    // used by the FFT hot path so frames that are intentionally not delivered
    // to QML never pay QVariant boxing cost. The caller may reuse `output`
    // across frames to keep heap churn low.
    void decodeScaledToVector(const QByteArray &data,
                              int skipSamples,
                              float scale,
                              QVector<float> &output);
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
