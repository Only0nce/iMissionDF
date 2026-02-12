// imaadpcmcodec.cpp
#include "pcmImaadpcmcodec.h"
#include <algorithm>
#include <cstring>

const int PCMImaAdpcmCodec::indexTable[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

const int PCMImaAdpcmCodec::stepTable[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

PCMImaAdpcmCodec::PCMImaAdpcmCodec() {
    syncWord = "SYNC";
    reset();
}

void PCMImaAdpcmCodec::reset() {
    stepIndex = 0;
    predictor = 0;
    step = 0;
    synchronized = 0;
    syncCounter = 0;
    phase = 0;
    syncBufferIndex = 0;
    std::memset(syncBuffer, 0, sizeof(syncBuffer));
}

qint16 PCMImaAdpcmCodec::decodeNibble(quint8 nibble) {
    step = stepTable[stepIndex];
    int diff = step >> 3;

    if (nibble & 1) diff += step >> 2;
    if (nibble & 2) diff += step >> 1;
    if (nibble & 4) diff += step;
    if (nibble & 8) diff = -diff;

    predictor += diff;
    predictor = std::clamp(predictor, -32768, 32767);

    stepIndex += indexTable[nibble];
    stepIndex = std::clamp(stepIndex, 0, 88);

    return static_cast<qint16>(predictor);
}

QVector<qint16> PCMImaAdpcmCodec::decodeWithSync(const QByteArray &data) {
    QVector<qint16> output;
    output.reserve(data.size() * 2);

    for (int i = 0; i < data.size(); ++i) {
        quint8 byte = static_cast<quint8>(data[i]);

        switch (phase) {
        case 0: // sync detection
            if (byte != static_cast<quint8>(syncWord[synchronized])) {
                synchronized = 0;
            } else if (++synchronized == syncWord.size()) {
                syncBufferIndex = 0;
                phase = 1;
            }
            break;

        case 1: // receive sync header (4 bytes)
            syncBuffer[syncBufferIndex++] = byte;
            if (syncBufferIndex == 4) {
                stepIndex = static_cast<qint16>(syncBuffer[0] | (syncBuffer[1] << 8));
                predictor = static_cast<qint16>(syncBuffer[2] | (syncBuffer[3] << 8));
                syncCounter = 1000;
                phase = 2;
            }
            break;

        case 2: // decode audio
            output.append(decodeNibble(byte & 0x0F));
            output.append(decodeNibble(byte >> 4));
            if (--syncCounter == 0) {
                synchronized = 0;
                phase = 0;
            }
            break;
        }
    }

    return output;
}
