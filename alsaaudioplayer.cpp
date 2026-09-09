// alsaaudioplayer.cpp
#include "alsaaudioplayer.h"
#include "CrashDiagnostics.h"
#include <QDebug>
#include <algorithm>

AlsaAudioPlayer::AlsaAudioPlayer(int sampleRate, int audioFormat, QObject *parent)
    : QObject(parent)
{
    // Keep the QObject in its owner thread. Only audioLoop itself executes on
    // m_thread via a direct started-signal connection.
    connect(&m_thread, &QThread::started,
            this, &AlsaAudioPlayer::audioLoop,
            Qt::DirectConnection);

    m_sampleRate = sampleRate;
    m_audioFormat = audioFormat;

    auto envInt = [](const char *name, int fallback, int minimum, int maximum) {
        bool ok = false;
        const int value = qgetenv(name).trimmed().toInt(&ok);
        if (!ok)
            return fallback;
        return std::max(minimum, std::min(maximum, value));
    };

    // Preserve the historical timing defaults in this backend-hardening
    // revision so normal audio UX does not change. Low-latency values such as
    // 40/10/20 ms can still be enabled explicitly for controlled A/B testing.
    m_bufferMs = envInt("ISCAN_AUDIO_BUFFER_MS", 100, 20, 250);
    m_periodMs = envInt("ISCAN_AUDIO_PERIOD_MS", 20, 5, 50);
    if (m_periodMs >= m_bufferMs)
        m_periodMs = std::max(5, m_bufferMs / 4);
    m_stageMs = envInt("ISCAN_AUDIO_STAGE_MS", 100, 5, 150);
    m_stageMs = std::min(m_stageMs, m_bufferMs);
    m_maxQueuedChunks = envInt("ISCAN_AUDIO_MAX_QUEUE_CHUNKS", 30, 3, 60);

    qInfo().noquote() << "[AUDIO CONFIG]"
                      << "rate=" << m_sampleRate
                      << "buffer_ms=" << m_bufferMs
                      << "period_ms=" << m_periodMs
                      << "stage_ms=" << m_stageMs
                      << "max_queue_chunks=" << m_maxQueuedChunks;
}


AlsaAudioPlayer::~AlsaAudioPlayer() {
    stop();
}
bool AlsaAudioPlayer::initAlsa()
{
    int err = 0;
    snd_pcm_hw_params_t *params = nullptr;

    auto fail = [this](const char *what, int code) {
        qWarning() << what << snd_strerror(code);
        if (m_pcmHandle) {
            snd_pcm_close(m_pcmHandle);
            m_pcmHandle = nullptr;
        }
        return false;
    };

    // Non-blocking writes keep stop()/shutdown independent from ALSA device
    // timing and make the audio thread the sole owner of m_pcmHandle.
    if ((err = snd_pcm_open(&m_pcmHandle, "default", SND_PCM_STREAM_PLAYBACK,
                            SND_PCM_NONBLOCK)) < 0)
        return fail("PCM open error:", err);

    snd_pcm_hw_params_alloca(&params);
    if ((err = snd_pcm_hw_params_any(m_pcmHandle, params)) < 0)
        return fail("Unable to initialize hw params:", err);
    if ((err = snd_pcm_hw_params_set_access(m_pcmHandle, params,
                                             SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
        return fail("Unable to set PCM access:", err);

    const snd_pcm_format_t format = (m_audioFormat == SND_PCM_FORMAT_S8)
        ? SND_PCM_FORMAT_S8 : SND_PCM_FORMAT_S16_LE;
    if ((err = snd_pcm_hw_params_set_format(m_pcmHandle, params, format)) < 0)
        return fail("Unable to set PCM format:", err);
    if ((err = snd_pcm_hw_params_set_channels(m_pcmHandle, params, 1)) < 0)
        return fail("Unable to set PCM channels:", err);

    unsigned int rate = static_cast<unsigned int>(m_sampleRate);
    if ((err = snd_pcm_hw_params_set_rate_near(m_pcmHandle, params, &rate, nullptr)) < 0)
        return fail("Unable to set PCM sample rate:", err);

    snd_pcm_uframes_t bufferSize = static_cast<snd_pcm_uframes_t>(
        qMax(1, (m_sampleRate * m_bufferMs) / 1000));
    if ((err = snd_pcm_hw_params_set_buffer_size_near(m_pcmHandle, params, &bufferSize)) < 0)
        return fail("Unable to set PCM buffer size:", err);

    snd_pcm_uframes_t periodSize = static_cast<snd_pcm_uframes_t>(
        qMax(1, (m_sampleRate * m_periodMs) / 1000));
    if ((err = snd_pcm_hw_params_set_period_size_near(m_pcmHandle, params,
                                                       &periodSize, nullptr)) < 0)
        return fail("Unable to set PCM period size:", err);

    if ((err = snd_pcm_hw_params(m_pcmHandle, params)) < 0)
        return fail("Unable to set hw params:", err);

    snd_pcm_hw_params_get_buffer_size(params, &bufferSize);
    snd_pcm_hw_params_get_period_size(params, &periodSize, nullptr);

    snd_pcm_sw_params_t *swParams = nullptr;
    snd_pcm_sw_params_alloca(&swParams);
    if (snd_pcm_sw_params_current(m_pcmHandle, swParams) >= 0) {
        const snd_pcm_uframes_t availMin = qMax<snd_pcm_uframes_t>(1, periodSize);
        const snd_pcm_uframes_t startThreshold =
            qMin(bufferSize, qMax<snd_pcm_uframes_t>(1, periodSize * 2));
        snd_pcm_sw_params_set_avail_min(m_pcmHandle, swParams, availMin);
        snd_pcm_sw_params_set_start_threshold(m_pcmHandle, swParams, startThreshold);
        snd_pcm_sw_params_set_stop_threshold(m_pcmHandle, swParams, bufferSize);
        if ((err = snd_pcm_sw_params(m_pcmHandle, swParams)) < 0)
            qWarning() << "Unable to apply ALSA sw params:" << snd_strerror(err);
    }

    if ((err = snd_pcm_prepare(m_pcmHandle)) < 0)
        return fail("Cannot prepare audio interface:", err);

    qInfo().noquote() << "[ALSA READY]"
                      << "requested_rate=" << m_sampleRate
                      << "actual_rate=" << rate
                      << "buffer_frames=" << static_cast<qulonglong>(bufferSize)
                      << "period_frames=" << static_cast<qulonglong>(periodSize);
    return true;
}
// bool AlsaAudioPlayer::initAlsa() {
//     snd_pcm_hw_params_t *params;
//     snd_pcm_open(&m_pcmHandle, "default", SND_PCM_STREAM_PLAYBACK, 0);
//     snd_pcm_hw_params_malloc(&params);
//     snd_pcm_hw_params_any(m_pcmHandle, params);
//     snd_pcm_hw_params_set_access(m_pcmHandle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
//     snd_pcm_hw_params_set_format(m_pcmHandle, params, SND_PCM_FORMAT_S16_LE);
//     snd_pcm_hw_params_set_channels(m_pcmHandle, params, 1);
//     unsigned int rate = 16000;
//     snd_pcm_hw_params_set_rate_near(m_pcmHandle, params, &rate, nullptr);

//     // Fix here: use a variable instead of a temporary
//     // snd_pcm_uframes_t bufferSize = 5120;
//     // snd_pcm_uframes_t periodSize = 1280;
//     // snd_pcm_hw_params_set_buffer_size_near(m_pcmHandle, params, &bufferSize);
//     // snd_pcm_hw_params_set_period_size_near(m_pcmHandle, params, &periodSize, nullptr);

//     if (snd_pcm_hw_params(m_pcmHandle, params) < 0) {
//         qWarning() << "Failed to set ALSA hardware parameters";
//         return false;
//     }

//     snd_pcm_hw_params_free(params);
//     snd_pcm_prepare(m_pcmHandle);
//     return true;
// }


void AlsaAudioPlayer::closeAlsa()
{
    if (!m_pcmHandle)
        return;

    snd_pcm_close(m_pcmHandle);
    m_pcmHandle = nullptr;
}

void AlsaAudioPlayer::start()
{
    if (m_running.load(std::memory_order_acquire))
        return;

    if (m_thread.isRunning()) {
        qWarning() << "Audio thread still stopping; start deferred";
        return;
    }

    m_running.store(true, std::memory_order_release);
    m_thread.setObjectName("AudioPlaybackThread");
    m_thread.start(QThread::TimeCriticalPriority);
}

void AlsaAudioPlayer::stop()
{
    m_running.store(false, std::memory_order_release);
    {
        QMutexLocker locker(&m_mutex);
        // Live receiver audio must never replay stale queued speech after a
        // reconnect or route restart. Keep only the live edge.
        m_queue.clear();
        m_dataAvailable.wakeAll();
    }

    // m_pcmHandle belongs exclusively to audioLoop(). With non-blocking ALSA
    // I/O there is no need for cross-thread snd_pcm_drop()/close(), removing
    // the previous handle lifetime race and QThread::terminate() fallback.
    if (m_thread.isRunning()) {
        m_thread.quit();
        m_thread.wait();
    }
}
// void AlsaAudioPlayer::start() {
//     if (!m_running) {
//         m_running = true;
//         m_thread.start();
//     }
// }

// void AlsaAudioPlayer::stop() {
//     if (m_running) {
//         m_running = false;
//         m_dataAvailable.wakeAll();
//         m_thread.quit();
//         m_thread.wait();
//         closeAlsa();
//     }
// }
void AlsaAudioPlayer::pushAudio(const QByteArray &data)
{
    CrashDiagnostics::checkpoint(CrashDiagnostics::CpAudioPush);
    if (data.isEmpty())
        return;

    // Lazy/recovery start: the first live packet opens ALSA, and a later packet
    // can restart playback if the worker exited after an unrecoverable device
    // error. This also avoids opening both 12 kHz and 16 kHz players merely on
    // WebSocket connection when only one route is active.
    if (!m_running.load(std::memory_order_acquire))
        start();

    QMutexLocker locker(&m_mutex);

    int dropped = 0;
    while (m_queue.size() >= m_maxQueuedChunks) {
        m_queue.dequeue();
        ++dropped;
    }
    if (dropped > 0)
        qWarning() << "Audio queue overflow - dropped" << dropped << "old chunks";

    m_queue.enqueue(data);
    m_dataAvailable.wakeOne();
    CrashDiagnostics::checkpoint(CrashDiagnostics::CpAudioPushDone);
}
// void AlsaAudioPlayer::pushAudio(const QByteArray &data)
// {
//     // qDebug() << "[pushAudio] PCM bytes:" << data.size();
//     QMutexLocker locker(&m_mutex);
//     m_queue.enqueue(data);
//     m_dataAvailable.wakeOne();
// }

// void AlsaAudioPlayer::audioLoop() {
//     if (!initAlsa()) {
//         qWarning() << "Failed to initialize ALSA.";
//         return;
//     }


//     while (m_running) {
//         QByteArray chunk;

//         {
//             QMutexLocker locker(&m_mutex);
//             if (m_queue.isEmpty()) {
//                 m_dataAvailable.wait(&m_mutex);
//                 if (!m_running) break;
//                 continue;
//             }
//             chunk = m_queue.dequeue();
//         }

//         // Accumulate PCM data
//         writeBuffer.append(chunk);

//         // Write to ALSA in 320-byte (160-frame) blocks
//         if (writeBuffer.size() > 4800)
//         {
//             int writeSize = qMin(writeBuffer.size(), 4800); // up to full buffer
//             int frames = writeSize / 2; // 2 bytes per sample

//             int err = snd_pcm_writei(m_pcmHandle, writeBuffer.constData(), frames);
//             if (err == -EPIPE) {
//                 snd_pcm_prepare(m_pcmHandle);
//                 qWarning() << QDateTime::currentDateTime().toString("hh:mm:ss") << "ALSA underrun recovered.";
//             } else if (err < 0) {
//                 qWarning() << "ALSA write error:" << snd_strerror(err);
//             } else {
//                 writeBuffer.remove(0, err * 2); // remove written bytes
//             }
//         }
//         // else
//         // qDebug() << "[audioLoop] writeBuffer PCM bytes:" << writeBuffer.size();


//     }

//     closeAlsa();
// }

// void AlsaAudioPlayer::audioLoop() {
//     if (!initAlsa()) {
//         qWarning() << "Failed to initialize ALSA.";
//         return;
//     }

//     QByteArray writeBuffer;

//     while (m_running) {
//         QByteArray chunk;

//         {
//             QMutexLocker locker(&m_mutex);
//             if (m_queue.isEmpty()) {
//                 // qDebug() << "Audio input queue empty!";
//                 m_dataAvailable.wait(&m_mutex);
//                 if (!m_running) break;
//                 continue;
//             }
//             chunk = m_queue.dequeue();
//         }

//         writeBuffer.append(chunk);

//         if (writeBuffer.size() >= 3200) {
//             int frames = 320;
//             int err = snd_pcm_writei(m_pcmHandle, writeBuffer.constData(), frames);
//             if (err == -EPIPE) {
//                 snd_pcm_prepare(m_pcmHandle);
//                 qWarning() << QDateTime::currentDateTime().toString("hh:mm:ss") << "ALSA underrun recovered.";
//             } else if (err < 0) {
//                 qWarning() << "ALSA write error:" << snd_strerror(err);
//             } else {
//                 writeBuffer.remove(0, err * 2); // remove written bytes
//                 QThread::msleep(8); // allow time to drain before next write
//             }
//         }

//     }

//     closeAlsa();
// }
void AlsaAudioPlayer::audioLoop()
{
    CrashDiagnostics::checkpoint(CrashDiagnostics::CpAudioLoop);
    // ALSA ownership can temporarily be unavailable during boot/service
    // transitions. Retry in the audio thread instead of leaving m_running=true
    // with a dead consumer or requiring an application restart.
    while (m_running.load(std::memory_order_acquire) && !initAlsa()) {
        qWarning() << "Failed to initialize ALSA; retrying in 1 second.";
        QMutexLocker locker(&m_mutex);
        m_dataAvailable.wait(&m_mutex, 1000);
    }

    if (!m_running.load(std::memory_order_acquire)) {
        m_thread.quit();
        return;
    }

    QByteArray localWriteBuffer;
    const int bytesPerFrame = (m_audioFormat == SND_PCM_FORMAT_S8) ? 1 : 2;
    const int targetFrames = qMax(1, (m_sampleRate * m_stageMs) / 1000);
    const int periodFrames = qMax(1, (m_sampleRate * m_periodMs) / 1000);
    const int targetBytes = targetFrames * bytesPerFrame;

    while (m_running.load(std::memory_order_acquire)) {
        while (localWriteBuffer.size() < targetBytes
               && m_running.load(std::memory_order_acquire)) {
            QMutexLocker locker(&m_mutex);
            if (m_queue.isEmpty()) {
                m_dataAvailable.wait(&m_mutex, qMax(10, m_periodMs * 2));
                if (m_queue.isEmpty())
                    break;
            }
            localWriteBuffer.append(m_queue.dequeue());
        }

        if (!m_running.load(std::memory_order_acquire))
            break;
        if (localWriteBuffer.size() < bytesPerFrame)
            continue;

        const int waitResult = snd_pcm_wait(m_pcmHandle, qMax(10, m_periodMs * 2));
        if (waitResult == 0)
            continue;
        if (waitResult < 0) {
            const int recover = snd_pcm_recover(m_pcmHandle, waitResult, 1);
            if (recover < 0) {
                qWarning() << "ALSA wait recovery failed:" << snd_strerror(recover);
                break;
            }
            continue;
        }

        const int availableFrames = localWriteBuffer.size() / bytesPerFrame;
        const int framesToWrite = qMin(availableFrames, periodFrames);
        CrashDiagnostics::checkpoint(CrashDiagnostics::CpAudioWrite);
        const snd_pcm_sframes_t written =
            snd_pcm_writei(m_pcmHandle, localWriteBuffer.constData(), framesToWrite);

        if (written == -EAGAIN)
            continue;

        if (written < 0) {
            const int recover = snd_pcm_recover(m_pcmHandle, static_cast<int>(written), 1);
            if (recover < 0) {
                qWarning() << "ALSA write recovery failed:" << snd_strerror(recover);
                break;
            }
            continue;
        }

        if (written > 0)
            localWriteBuffer.remove(0, static_cast<int>(written) * bytesPerFrame);
    }

    if (m_pcmHandle)
        snd_pcm_drop(m_pcmHandle);
    closeAlsa();
    m_running.store(false, std::memory_order_release);
    m_thread.quit();
}
// int AlsaAudioPlayer::queueSize() const {
//     QMutexLocker locker(&m_mutex);
//     return m_queue.size();
// }

// int AlsaAudioPlayer::bufferLevel() const {
//     snd_pcm_sframes_t delay;
//     if (snd_pcm_delay(m_pcmHandle, &delay) == 0) {
//         return delay;
//     }
//     return -1;
// }
