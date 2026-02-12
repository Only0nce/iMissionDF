#ifndef ALSAAUDIOPLAYER_H
#define ALSAAUDIOPLAYER_H

// alsaaudioplayer.h
#pragma once

#include <QObject>
#include <QByteArray>
#include <QThread>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>
#include <alsa/asoundlib.h>
#include <speex/speex_resampler.h>
#include <QVector>
#include <QDebug>
#include <QDateTime>

class AlsaAudioPlayer : public QObject {
    Q_OBJECT
public:
    explicit AlsaAudioPlayer(int sampleRate, int audioFormat, QObject *parent = nullptr);
    ~AlsaAudioPlayer();

    void pushAudio(const QByteArray &data);
    void start();
    void stop();

    QVector<qint16> resampleTo8000(const QVector<qint16> &input, int inRate = 16000, int outRate = 8000) {
        QVector<qint16> output;
        output.resize((input.size() * outRate) / inRate + 10); // conservative estimate

        int err;
        SpeexResamplerState *resampler = speex_resampler_init(1, inRate, outRate, SPEEX_RESAMPLER_QUALITY_DEFAULT, &err);
        if (err != RESAMPLER_ERR_SUCCESS) {
            qWarning() << "Resampler init failed:" << speex_resampler_strerror(err);
            return {};
        }

        spx_uint32_t inLen = input.size();
        spx_uint32_t outLen = output.size();

        int ret = speex_resampler_process_int(resampler, 0,
                                              reinterpret_cast<const spx_int16_t*>(input.constData()), &inLen,
                                              reinterpret_cast<spx_int16_t*>(output.data()), &outLen);
        if (ret != RESAMPLER_ERR_SUCCESS) {
            qWarning() << "Resampling failed:" << speex_resampler_strerror(ret);
        }

        speex_resampler_destroy(resampler);
        output.resize(outLen); // trim unused samples
        return output;
    }

private:
    QThread m_thread;
    QMutex m_mutex;
    QWaitCondition m_dataAvailable;
    QQueue<QByteArray> m_queue;
    bool m_running = false;
    QByteArray audioBuffer;

    QByteArray writeBuffer;

    snd_pcm_t *m_pcmHandle = nullptr;
    void audioLoop();
    bool initAlsa();
    void closeAlsa();
    int m_sampleRate = 16000;
    int m_audioFormat = SND_PCM_FORMAT_S16_LE;
};

#endif // ALSAAUDIOPLAYER_H
