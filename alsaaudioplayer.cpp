// alsaaudioplayer.cpp
#include "alsaaudioplayer.h"
#include <QDebug>

AlsaAudioPlayer::AlsaAudioPlayer(int sampleRate, int audioFormat, QObject *parent) : QObject(parent) {
    moveToThread(&m_thread);
    connect(&m_thread, &QThread::started, this, &AlsaAudioPlayer::audioLoop);
    m_sampleRate = sampleRate;
    m_audioFormat = audioFormat;
}

AlsaAudioPlayer::~AlsaAudioPlayer() {
    stop();
}
bool AlsaAudioPlayer::initAlsa() {
    int err;
    snd_pcm_hw_params_t *params;

    // Open PCM device
    if ((err = snd_pcm_open(&m_pcmHandle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        qWarning() << "PCM open error:" << snd_strerror(err);
        return false;
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(m_pcmHandle, params);

    // Set hardware parameters
    snd_pcm_hw_params_set_access(m_pcmHandle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (m_audioFormat == SND_PCM_FORMAT_S16_LE)
        snd_pcm_hw_params_set_format(m_pcmHandle, params, SND_PCM_FORMAT_S16_LE);
    else if (m_audioFormat == SND_PCM_FORMAT_S8)
        snd_pcm_hw_params_set_format(m_pcmHandle, params, SND_PCM_FORMAT_S8);
    snd_pcm_hw_params_set_channels(m_pcmHandle, params, 1);

    unsigned int rate = m_sampleRate;
    snd_pcm_hw_params_set_rate_near(m_pcmHandle, params, &rate, nullptr);

    // Set buffer for 100ms of audio (1600 frames)
    snd_pcm_uframes_t buffer_size = 1600;
    snd_pcm_hw_params_set_buffer_size_near(m_pcmHandle, params, &buffer_size);

    // Set period to 20ms (320 frames)
    snd_pcm_uframes_t period_size = 320;
    snd_pcm_hw_params_set_period_size_near(m_pcmHandle, params, &period_size, nullptr);

    if ((err = snd_pcm_hw_params(m_pcmHandle, params)) < 0) {
        qWarning() << "Unable to set hw params:" << snd_strerror(err);
        return false;
    }

    // Get actual parameters
    snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
    snd_pcm_hw_params_get_period_size(params, &period_size, nullptr);
    qDebug() << "ALSA configured - Buffer size:" << buffer_size << "frames, Period size:" << period_size << "frames";

    if ((err = snd_pcm_prepare(m_pcmHandle)) < 0) {
        qWarning() << "Cannot prepare audio interface:" << snd_strerror(err);
        return false;
    }

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


void AlsaAudioPlayer::closeAlsa() {
    if (m_pcmHandle) {
        snd_pcm_drain(m_pcmHandle);
        snd_pcm_close(m_pcmHandle);
        m_pcmHandle = nullptr;
    }
}
void AlsaAudioPlayer::start() {
    if (!m_running) {
        m_running = true;
        m_thread.setObjectName("AudioPlaybackThread");
        m_thread.start(QThread::TimeCriticalPriority);
    }
}

void AlsaAudioPlayer::stop() {
    if (m_running) {
        m_running = false;
        {
            QMutexLocker locker(&m_mutex);
            m_dataAvailable.wakeAll();
        }
        m_thread.quit();
        if (!m_thread.wait(1000)) {
            qWarning() << "Failed to stop audio thread gracefully";
            m_thread.terminate();
            m_thread.wait();
        }
        closeAlsa();
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
void AlsaAudioPlayer::pushAudio(const QByteArray &data) {
    QMutexLocker locker(&m_mutex);
    m_queue.enqueue(data);
    if (m_queue.size() > 5) {  // More than 5 chunks waiting
        // qWarning() << "Audio queue building up:" << m_queue.size() << "chunks";
    }
    m_dataAvailable.wakeOne();
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
void AlsaAudioPlayer::audioLoop() {
    if (!initAlsa()) {
        qWarning() << "Failed to initialize ALSA.";
        // emit playbackError("ALSA initialization failed");
        return;
    }

    QByteArray writeBuffer;
    const int targetBufferSize = 1600; // 100ms buffer
    auto lastWriteTime = std::chrono::steady_clock::now();

    while (m_running) {
        // Fill buffer to target size
        while (writeBuffer.size() < targetBufferSize && m_running) {
            QMutexLocker locker(&m_mutex);
            if (m_queue.isEmpty()) {
                m_dataAvailable.wait(&m_mutex, 100);
                if (m_queue.isEmpty()) break;
            }

            // Don't let the queue grow too large
            if (m_queue.size() > 30) {
                qWarning() << QTime::currentTime().toString("hh:mm:ss") << "Queue overflow - dropping" << (m_queue.size() - 30) << "chunks";
                while (m_queue.size() > 30) {
                    m_queue.dequeue();
                }
            }

            writeBuffer.append(m_queue.dequeue());
        }

        // Write to ALSA with precise timing
        if (!writeBuffer.isEmpty()) {
            int framesToWrite = qMin(writeBuffer.size() / 2, 320); // Write 20ms chunks
            int err = snd_pcm_writei(m_pcmHandle, writeBuffer.constData(), framesToWrite);

            if (err == -EPIPE) {
                // qWarning() << "Underrun occurred - recovering";
                snd_pcm_recover(m_pcmHandle, err, 1);
                continue;
            } else if (err < 0) {
                qWarning() << "Write error:" << snd_strerror(err);
                break;
            }

            // Remove written data
            writeBuffer.remove(0, err * 2);

            // Calculate exact sleep time
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastWriteTime);
            int expectedInterval = (framesToWrite * 1000) / 16000;
            int remaining = expectedInterval - elapsed.count();

            if (remaining > 0) {
                // qDebug() << "remaining" << remaining;
                QThread::msleep(remaining);
            }
            lastWriteTime = now;
        }
    }

    closeAlsa();
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
