// websocketclient.cpp
#include "websocketclient.h"
#include "CrashDiagnostics.h"
#include "pcmImaadpcmcodec.h"
#include <QDebug>
#include <QMetaMethod>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <limits>

namespace {
bool envEnabled(const char *name, bool defaultValue)
{
    const QByteArray value = qgetenv(name).trimmed().toLower();
    if (value.isEmpty())
        return defaultValue;
    if (value == "1" || value == "true" || value == "yes" || value == "on")
        return true;
    if (value == "0" || value == "false" || value == "no" || value == "off")
        return false;
    return defaultValue;
}

int envPositiveInt(const char *name, int defaultValue, int maxValue)
{
    bool ok = false;
    const int parsed = qgetenv(name).trimmed().toInt(&ok);
    if (!ok || parsed <= 0)
        return defaultValue;
    return qBound(1, parsed, maxValue);
}
}
WebSocketClient::WebSocketClient(QObject *parent) : QObject(parent)
{
    // R20.4 / R18 restore: production FFT rendering is native C++.
    // The legacy QVariantList/QML bridge is diagnostic opt-in only.
    m_fftQmlPublish = envEnabled("ISCAN_FFT_QML_PUBLISH", false);
    m_fftStrictSize = envEnabled("ISCAN_FFT_STRICT_SIZE", true);
    m_fftMaxBins = envPositiveInt("ISCAN_FFT_MAX_BINS", 65536, 1048576);
    m_spectrumDisplayBins = envPositiveInt("ISCAN_SPECTRUM_DISPLAY_BINS", 8192, m_fftMaxBins);
    m_waterfallDisplayBins = envPositiveInt("ISCAN_WATERFALL_DISPLAY_BINS", 1280, m_fftMaxBins);

    qInfo().noquote() << "[R20.4 FFT Runtime]"
                      << "qmlPublish=" << m_fftQmlPublish
                      << "strictSize=" << m_fftStrictSize
                      << "maxBins=" << m_fftMaxBins
                      << "spectrumDisplayBins=" << m_spectrumDisplayBins
                      << "waterfallDisplayBins=" << m_waterfallDisplayBins
                      << "renderer=native-qquickpainteditem";
    // Connect Qt signals exactly once. connectToServer() may be called again for
    // reconnect/target changes without multiplying message handlers.
    connect(&webSocket, &QWebSocket::connected,
            this, &WebSocketClient::onConnected, Qt::UniqueConnection);
    connect(&webSocket, &QWebSocket::textMessageReceived,
            this, &WebSocketClient::onTextMessageReceived, Qt::UniqueConnection);
    connect(&webSocket, &QWebSocket::binaryMessageReceived,
            this, &WebSocketClient::onBinaryMessageReceived, Qt::UniqueConnection);
    connect(&resetSQL, &QTimer::timeout,
            this, &WebSocketClient::resetSQLCount, Qt::UniqueConnection);
    resetSQL.setInterval(100);

    connect(&webSocket, &QWebSocket::disconnected,
            this, &WebSocketClient::onDisconnected, Qt::UniqueConnection);
    connect(&webSocket,
            static_cast<void (QWebSocket::*)(QAbstractSocket::SocketError)>(&QWebSocket::error),
            this, &WebSocketClient::onSocketError, Qt::UniqueConnection);

    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout,
            this, &WebSocketClient::attemptReconnect, Qt::UniqueConnection);
}

WebSocketClient::~WebSocketClient()
{
    m_shuttingDown = true;
    m_reconnectTimer.stop();
    resetSQL.stop();
    if (webSocket.state() != QAbstractSocket::UnconnectedState)
        webSocket.close();

    // These players are created by WebSocketClient in this source tree.
    // Stop their QThreads before process teardown so Qt never destroys a
    // running playback thread.
    if (hdAudioPlayer) {
        hdAudioPlayer->stop();
        delete hdAudioPlayer;
        hdAudioPlayer = nullptr;
    }
    if (sdAudioPlayer) {
        sdAudioPlayer->stop();
        delete sdAudioPlayer;
        sdAudioPlayer = nullptr;
    }
}

void WebSocketClient::setFftUiActive(bool active)
{
    if (m_fftUiActive == active)
        return;

    m_fftUiActive = active;

    // Make the first frame after entering the Spectrum page immediate.
    m_fftUiPublishTimer.invalidate();
    m_spectrumDisplayTimer.invalidate();
    m_waterfallDisplayTimer.invalidate();
    m_fftAutoScaleTimer.invalidate();
    m_fftAutoScaleValid = false;
    emit fftAutoScaleStatsChanged();

    qInfo().noquote() << "[FFT Runtime]"
                      << (m_fftUiActive ? "ACTIVE" : "SUSPENDED")
                      << "(audio remains active)";
    emit fftUiActiveChanged(m_fftUiActive);
}

void WebSocketClient::setMaxHoldEnabled(bool enabled)
{
    if (m_maxHoldEnabled == enabled)
        return;

    m_maxHoldEnabled = enabled;
    if (m_maxHoldEnabled && !m_maxHoldPublishTimer.isValid())
        m_maxHoldPublishTimer.start();

    emit maxHoldEnabledChanged(m_maxHoldEnabled);
}

void WebSocketClient::resetMaxHold()
{
    m_maxHold.clear();
    if (m_maxHoldPublishTimer.isValid())
        m_maxHoldPublishTimer.restart();

    emit maxHoldDisplayFrame(QVector<float>());

    static const QMetaMethod legacySignal =
            QMetaMethod::fromSignal(&WebSocketClient::maxHoldUpdated);
    if (isSignalConnected(legacySignal))
        emit maxHoldUpdated(QVariantList());
}

QVariantList WebSocketClient::maxHoldToVariantList() const
{
    QVariantList snapshot;
    snapshot.reserve(m_maxHold.size());
    for (float value : m_maxHold)
        snapshot.append(value);
    return snapshot;
}

QVariantList WebSocketClient::maxHoldSnapshot() const
{
    return maxHoldToVariantList();
}

bool WebSocketClient::shouldPublishFftUiFrame()
{
    if (!m_fftQmlPublish)
        return false;
    if (!m_fftUiPublishTimer.isValid()) {
        m_fftUiPublishTimer.start();
        return true;
    }
    if (m_fftUiPublishTimer.elapsed() >= m_fftUiPublishIntervalMs) {
        m_fftUiPublishTimer.restart();
        return true;
    }
    return false;
}

bool WebSocketClient::shouldPublishSpectrumFrame()
{
    if (!m_spectrumDisplayTimer.isValid()) {
        m_spectrumDisplayTimer.start();
        return true;
    }
    if (m_spectrumDisplayTimer.elapsed() >= m_spectrumDisplayIntervalMs) {
        m_spectrumDisplayTimer.restart();
        return true;
    }
    return false;
}

bool WebSocketClient::shouldPublishWaterfallFrame()
{
    if (!m_waterfallDisplayTimer.isValid()) {
        m_waterfallDisplayTimer.start();
        return true;
    }
    if (m_waterfallDisplayTimer.elapsed() >= m_waterfallDisplayIntervalMs) {
        m_waterfallDisplayTimer.restart();
        return true;
    }
    return false;
}

bool WebSocketClient::validateFftFrame(const QVector<float> &fftFrame, int payloadBytes)
{
    static quint64 rejectCount = 0;
    QString reason;

    if (payloadBytes >= 0 && (payloadBytes % static_cast<int>(sizeof(float))) != 0) {
        reason = QStringLiteral("unaligned-payload");
    } else if (fftFrame.isEmpty()) {
        reason = QStringLiteral("empty");
    } else if (fftFrame.size() > m_fftMaxBins) {
        reason = QStringLiteral("too-many-bins");
    } else if (m_fftStrictSize && rxconfig.fft_size > 0
               && fftFrame.size() != rxconfig.fft_size) {
        reason = QStringLiteral("fft-size-mismatch");
    } else {
        for (float value : fftFrame) {
            if (!std::isfinite(value)) {
                reason = QStringLiteral("non-finite");
                break;
            }
        }
    }

    if (reason.isEmpty())
        return true;

    ++rejectCount;
    if (rejectCount <= 8 || (rejectCount % 100) == 0) {
        qWarning().noquote() << "[R20.4 FFT REJECT]"
                             << "reason=" << reason
                             << "bins=" << fftFrame.size()
                             << "expected=" << rxconfig.fft_size
                             << "payloadBytes=" << payloadBytes
                             << "count=" << rejectCount;
    }
    return false;
}

QVector<float> WebSocketClient::peakPoolForDisplay(const QVector<float> &source, int limit) const
{
    if (source.isEmpty() || limit <= 0)
        return QVector<float>();
    if (source.size() <= limit)
        return source;

    QVector<float> reduced;
    reduced.resize(limit);
    const int count = source.size();
    for (int out = 0; out < limit; ++out) {
        const int begin = static_cast<int>((static_cast<qint64>(out) * count) / limit);
        int end = static_cast<int>((static_cast<qint64>(out + 1) * count) / limit);
        end = qBound(begin + 1, end, count);

        float peak = source.at(begin);
        for (int i = begin + 1; i < end; ++i)
            peak = std::max(peak, source.at(i));
        reduced[out] = peak;
    }
    return reduced;
}

QVariantList WebSocketClient::fftToVariantList(const QVector<float> &fftFrame) const
{
    QVariantList list;
    list.reserve(fftFrame.size());
    for (float value : fftFrame)
        list.append(value);
    return list;
}

void WebSocketClient::updateFftAutoScaleStats(const QVector<float> &fftFrame)
{
    if (fftFrame.size() < 8)
        return;

    if (!m_fftAutoScaleTimer.isValid()) {
        m_fftAutoScaleTimer.start();
    } else if (m_fftAutoScaleTimer.elapsed() < m_fftAutoScaleIntervalMs) {
        return;
    } else {
        m_fftAutoScaleTimer.restart();
    }

    const int maxSamples = 512;
    const int stride = std::max(1, fftFrame.size() / maxSamples);
    m_fftAutoScaleScratch.clear();
    m_fftAutoScaleScratch.reserve(std::min(maxSamples + 1, fftFrame.size()));
    for (int i = 0; i < fftFrame.size(); i += stride) {
        const float value = fftFrame.at(i);
        if (std::isfinite(value))
            m_fftAutoScaleScratch.append(value);
    }
    if (m_fftAutoScaleScratch.size() < 8)
        return;

    std::sort(m_fftAutoScaleScratch.begin(), m_fftAutoScaleScratch.end());
    const int last = m_fftAutoScaleScratch.size() - 1;
    const int medianIndex = qBound(0, static_cast<int>(std::floor(0.50 * last)), last);
    const int strongIndex = qBound(0, static_cast<int>(std::floor(0.995 * last)), last);
    const double noise = m_fftAutoScaleScratch.at(medianIndex);
    const double strong = m_fftAutoScaleScratch.at(strongIndex);

    if (!std::isfinite(noise) || !std::isfinite(strong))
        return;

    const bool changed = !m_fftAutoScaleValid
            || !qFuzzyCompare(m_fftNoiseDb + 1.0, noise + 1.0)
            || !qFuzzyCompare(m_fftStrongDb + 1.0, strong + 1.0);
    m_fftNoiseDb = noise;
    m_fftStrongDb = strong;
    m_fftAutoScaleValid = true;
    if (changed)
        emit fftAutoScaleStatsChanged();
}

void WebSocketClient::publishNativeFftFrames(const QVector<float> &fftFrame)
{
    if (!m_fftUiActive || fftFrame.isEmpty())
        return;

    updateFftAutoScaleStats(fftFrame);

    if (shouldPublishSpectrumFrame())
        emit spectrumDisplayFrame(peakPoolForDisplay(fftFrame, m_spectrumDisplayBins));

    if (shouldPublishWaterfallFrame())
        emit waterfallDisplayFrame(peakPoolForDisplay(fftFrame, m_waterfallDisplayBins));

    if (shouldPublishFftUiFrame()) {
        const QVariantList legacy = fftToVariantList(fftFrame);
        emit fftFrameUpdated(legacy);

        static const QMetaMethod spectrumSignal =
                QMetaMethod::fromSignal(&WebSocketClient::spectrumUpdated);
        static const QMetaMethod waterfallSignal =
                QMetaMethod::fromSignal(&WebSocketClient::waterfallUpdated);
        if (isSignalConnected(spectrumSignal))
            emit spectrumUpdated(legacy);
        if (isSignalConnected(waterfallSignal))
            emit waterfallUpdated(legacy);
    }
}

void WebSocketClient::prepareMaxHold(int count)
{
    if (!m_maxHoldEnabled || count <= 0)
        return;

    if (m_maxHold.size() != count) {
        m_maxHold.resize(count);
        std::fill(m_maxHold.begin(),
                  m_maxHold.end(),
                  -std::numeric_limits<float>::infinity());
    }
}

void WebSocketClient::updateMaxHoldValue(int index, float value)
{
    if (!m_maxHoldEnabled
            || index < 0
            || index >= m_maxHold.size()
            || !std::isfinite(value))
        return;

    if (value > m_maxHold[index])
        m_maxHold[index] = value;
}

void WebSocketClient::publishMaxHoldIfDue()
{
    if (!m_maxHoldEnabled || m_maxHold.isEmpty())
        return;

    if (!m_maxHoldPublishTimer.isValid())
        m_maxHoldPublishTimer.start();

    if (m_maxHoldPublishTimer.elapsed() >= m_maxHoldPublishIntervalMs) {
        emit maxHoldDisplayFrame(peakPoolForDisplay(m_maxHold, m_spectrumDisplayBins));

        static const QMetaMethod legacySignal =
                QMetaMethod::fromSignal(&WebSocketClient::maxHoldUpdated);
        if (isSignalConnected(legacySignal))
            emit maxHoldUpdated(maxHoldToVariantList());

        m_maxHoldPublishTimer.restart();
    }
}

void WebSocketClient::updateMaxHold(const QVector<float> &fftFrame)
{
    if (!m_maxHoldEnabled || fftFrame.isEmpty())
        return;

    prepareMaxHold(fftFrame.size());
    for (int i = 0; i < fftFrame.size(); ++i)
        updateMaxHoldValue(i, fftFrame.at(i));

    publishMaxHoldIfDue();
}

void WebSocketClient::connectToServer(const QUrl &url)
{
    if (!url.isValid()) {
        qWarning() << "[ASTRARX-BACKEND] invalid WebSocket URL:" << url;
        return;
    }

    m_shuttingDown = false;
    m_targetUrl = url;
    m_reconnectDelayMs = 1000;
    m_reconnectTimer.stop();

    m_explicitSquelchSeen = false;
    sqlCount = 3;
    if (!resetSQL.isActive())
        resetSQL.start();

    if (webSocket.state() == QAbstractSocket::UnconnectedState) {
        openBackendSocket();
    } else {
        qInfo() << "[ASTRARX-BACKEND] closing previous socket before target reopen";
        webSocket.close();
        scheduleReconnect(QStringLiteral("target-change"));
    }
}

void WebSocketClient::openBackendSocket()
{
    if (m_shuttingDown || !m_targetUrl.isValid())
        return;
    if (webSocket.state() != QAbstractSocket::UnconnectedState)
        return;

    qInfo() << "[ASTRARX-BACKEND] opening" << m_targetUrl;
    webSocket.open(m_targetUrl);
}

void WebSocketClient::scheduleReconnect(const QString &reason)
{
    if (m_shuttingDown || !m_targetUrl.isValid())
        return;
    if (webSocket.state() == QAbstractSocket::ConnectedState)
        return;
    if (m_reconnectTimer.isActive())
        return;

    const int delayMs = qBound(1000, m_reconnectDelayMs, 15000);
    qWarning() << "[ASTRARX-BACKEND] reconnect scheduled"
               << "reason=" << reason
               << "delay_ms=" << delayMs;
    m_reconnectTimer.start(delayMs);
    m_reconnectDelayMs = qMin(15000, delayMs * 2);
}

void WebSocketClient::attemptReconnect()
{
    if (m_shuttingDown || webSocket.state() == QAbstractSocket::ConnectedState)
        return;

    if (webSocket.state() != QAbstractSocket::UnconnectedState) {
        scheduleReconnect(QStringLiteral("socket-not-yet-unconnected"));
        return;
    }

    openBackendSocket();
}

void WebSocketClient::onDisconnected()
{
    qWarning() << "[ASTRARX-BACKEND] disconnected";

    // Release local playback resources and stale queued speech while the RF
    // backend is unavailable. Reconnect is transparent to QML.
    if (hdAudioPlayer)
        hdAudioPlayer->stop();
    if (sdAudioPlayer)
        sdAudioPlayer->stop();

    scheduleReconnect(QStringLiteral("disconnected"));
}

void WebSocketClient::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    qWarning() << "[ASTRARX-BACKEND] socket error:" << webSocket.errorString();
    scheduleReconnect(QStringLiteral("socket-error"));
}

void WebSocketClient::onConnected() {
    m_reconnectTimer.stop();
    m_reconnectDelayMs = 1000;
    m_explicitSquelchSeen = false;
    qDebug() << "Connected to receiver backend.";
    webSocket.sendTextMessage("SERVER DE CLIENT client=openwebrx.js type=receiver");
    sendConnectionProperties(12000, 16000);
    // sendDspControl(-75000, 75000, 0, "wfm", 3, -1250000, -150, false);
    sendDspAction("start") ;

    // Audio players start lazily on the first type-2/type-4 packet. This keeps
    // the wire/UI contract unchanged while avoiding two unconditional ALSA opens
    // at every backend connection.
    qDebug() << "address hdAudioPlayer::s" << hdAudioPlayer;
    emit openwebrxConnected();
}
void WebSocketClient::resetSQLCount()
{
    if (m_explicitSquelchSeen)
        return;

    sqlCount++;
    if(sqlCount == 3)
    {
        if (sqlOn){
            sqlOn = false;
            emit onSQLChanged(sqlOn);
        }
        sqlCount = 3;
    }
}
void WebSocketClient::onBinaryMessageReceived(const QByteArray &message)
{
    CrashDiagnostics::checkpoint(CrashDiagnostics::CpWsBinary);
    if (message.isEmpty())
        return;

    const quint8 type = static_cast<quint8>(message.at(0));

    // Page-scoped FFT optimization: primary FFT is discarded before decode,
    // allocation, QVariant boxing and QML delivery when the Spectrum page is
    // inactive. Audio frame types 2/4 continue unchanged.
    if (!m_fftUiActive && type == 1)
        return;

    // Secondary FFT currently has no active consumer in this project. The old
    // path decoded a full frame into secondaryF32 and immediately discarded it
    // because secondary_demod_waterfall_add() is disabled. Drop it before the
    // payload copy/decode. Re-enable only together with a real consumer.
    if (type == 3)
        return;

    switch (type)
    {
    case 1: { // primary full-span FFT
        const int payloadBytes = message.size() - 1;
        if (payloadBytes <= 0)
            break;

        m_fftDecodeScratch.clear();

        if (rxconfig.fft_compression == "none") {
            if ((payloadBytes % static_cast<int>(sizeof(float))) != 0) {
                // validateFftFrame() also documents this contract, but reject
                // before allocating an impossible partial float vector.
                static quint64 unalignedRejects = 0;
                ++unalignedRejects;
                if (unalignedRejects <= 8 || (unalignedRejects % 100) == 0) {
                    qWarning() << "[R20.4 FFT REJECT] unaligned-payload"
                               << "bytes=" << payloadBytes
                               << "count=" << unalignedRejects;
                }
                break;
            }

            const int sampleCount = payloadBytes / static_cast<int>(sizeof(float));
            if (sampleCount <= 0 || sampleCount > m_fftMaxBins) {
                qWarning() << "[R20.4 FFT REJECT] invalid-bin-count"
                           << "bins=" << sampleCount
                           << "max=" << m_fftMaxBins;
                break;
            }

            m_fftDecodeScratch.resize(sampleCount);
            std::memcpy(m_fftDecodeScratch.data(),
                        message.constData() + 1,
                        static_cast<size_t>(sampleCount) * sizeof(float));
        }
        else if (rxconfig.fft_compression == "adpcm") {
            fft_codec.reset();
            const QByteArray payloadView = QByteArray::fromRawData(
                        message.constData() + 1,
                        payloadBytes);
            fft_codec.decodeScaledToVector(payloadView,
                                           COMPRESS_FFT_PAD_N,
                                           0.01f,
                                           m_fftDecodeScratch);
        }
        else {
            qWarning() << "[R20.4 FFT REJECT] unsupported compression"
                       << rxconfig.fft_compression;
            break;
        }

        if (!validateFftFrame(m_fftDecodeScratch,
                              rxconfig.fft_compression == "none" ? payloadBytes : -1))
            break;

        // Full-resolution native Max Hold remains exact. Display frames are
        // reduced only after this stage.
        updateMaxHold(m_fftDecodeScratch);

        // Production path: QVector<float> -> native QQuickPaintedItem.
        // Legacy QVariantList/QML publication is disabled unless explicitly
        // enabled with ISCAN_FFT_QML_PUBLISH=1.
        publishNativeFftFrames(m_fftDecodeScratch);
        break;
    }

    case 2: { // SD audio data
        QByteArray data = message.mid(1);
        if (rxconfig.audio_compression == "none") {
            const int sampleCount = data.size() / static_cast<int>(sizeof(qint16));
            if (sampleCount > 0) {
                applySoftwareVolume(data);

                if (sdAudioPlayer)
                    sdAudioPlayer->pushAudio(data);
                CrashDiagnostics::checkpoint(CrashDiagnostics::CpWsAudioPostPush);
            } else {
                qWarning() << "Invalid PCM size:" << data.size();
            }

            if (!m_explicitSquelchSeen) {
                sqlCount = 0;
                if (!sqlOn) {
                sqlOn = true;
                emit onSQLChanged(sqlOn);
                    qDebug() << "sd audio data"
                             << "data" << data.size()
                             << "rxconfig.audio_compression" << rxconfig.audio_compression;
                }
            }
        }
        break;
    }

    case 4: { // HD audio data
        QByteArray data = message.mid(1);
        if (rxconfig.audio_compression == "none") {
            const int sampleCount = data.size() / static_cast<int>(sizeof(qint16));
            if (sampleCount > 0) {
                applySoftwareVolume(data);

                if (hdAudioPlayer)
                    hdAudioPlayer->pushAudio(data);
                CrashDiagnostics::checkpoint(CrashDiagnostics::CpWsAudioPostPush);
            } else {
                qWarning() << "Invalid PCM size:" << data.size();
            }
        }
        else if (rxconfig.audio_compression == "adpcm") {
            PCMImaAdpcmCodec decoder;
            const QVector<qint16> pcmSamples = decoder.decodeWithSync(data);

            QByteArray pcm(reinterpret_cast<const char*>(pcmSamples.constData()),
                           pcmSamples.size() * static_cast<int>(sizeof(qint16)));
            applySoftwareVolume(pcm);

            if (hdAudioPlayer)
                hdAudioPlayer->pushAudio(pcm);
            CrashDiagnostics::checkpoint(CrashDiagnostics::CpWsAudioPostPush);
        }

        if (!m_explicitSquelchSeen) {
            sqlCount = 0;
            if (!sqlOn) {
                sqlOn = true;
                qDebug() << "hd audio data"
                         << "data" << data.size()
                         << "rxconfig.audio_compression" << rxconfig.audio_compression;
                emit onSQLChanged(sqlOn);
            }
        }
        break;
    }

    default:
        qWarning() << "Unknown binary message type:" << type;
        break;
    }
    CrashDiagnostics::checkpoint(CrashDiagnostics::CpWsBinaryReturn);
}

void WebSocketClient::handleConfigMessage(const QJsonObject &config)
{
    const quint64 oldCenter = rxconfig.center_freq;
    const quint64 oldStart = rxconfig.start_freq;
    const int oldOffset = rxconfig.start_offset_freq;
    const int oldSampleRate = rxconfig.samp_rate;
    const QString oldMode = rxconfig.start_mod;

    const bool hasCenter = config.contains("center_freq");
    const bool hasStart = config.contains("start_freq");
    const bool hasOffset = config.contains("start_offset_freq");

    rxconfig.fromJson(config);

    // AstraRX normally sends center + start_freq + offset together, while older
    // OpenWebRX-compatible messages may omit one member. Normalize the snapshot
    // so all downstream code sees one internally consistent receiver state.
    if (hasStart && !hasOffset && rxconfig.center_freq > 0) {
        // A receiver-only update is valid too; derive the DSP offset from the
        // latest known RF center even when this partial config omits center_freq.
        const qint64 derived = static_cast<qint64>(rxconfig.start_freq)
                             - static_cast<qint64>(rxconfig.center_freq);
        rxconfig.start_offset_freq = static_cast<int>(
            qBound<qint64>(std::numeric_limits<int>::min(),
                           derived,
                           std::numeric_limits<int>::max()));
    } else if (!hasStart && (hasCenter || hasOffset)) {
        const qint64 receiver = static_cast<qint64>(rxconfig.center_freq)
                              + static_cast<qint64>(rxconfig.start_offset_freq);
        rxconfig.start_freq = receiver > 0 ? static_cast<quint64>(receiver) : 0;
    }

    if (hasCenter && hasStart && hasOffset) {
        const qint64 expected = static_cast<qint64>(rxconfig.center_freq)
                              + static_cast<qint64>(rxconfig.start_offset_freq);
        const qint64 error = static_cast<qint64>(rxconfig.start_freq) - expected;
        if (std::llabs(error) > 1) {
            qWarning() << "[QT5-FREQ-RX] inconsistent server state"
                       << "center=" << rxconfig.center_freq
                       << "offset=" << rxconfig.start_offset_freq
                       << "start_freq=" << rxconfig.start_freq
                       << "error=" << error;
            // Absolute receiver frequency is authoritative for the UI. Rebuild
            // offset from it so overlay/readout never disagree.
            const qint64 normalized = static_cast<qint64>(rxconfig.start_freq)
                                    - static_cast<qint64>(rxconfig.center_freq);
            rxconfig.start_offset_freq = static_cast<int>(
                qBound<qint64>(std::numeric_limits<int>::min(),
                               normalized,
                               std::numeric_limits<int>::max()));
        }
    }

    const bool centerMetaChanged =
        rxconfig.center_freq != oldCenter
        || rxconfig.samp_rate != oldSampleRate
        || rxconfig.start_mod != oldMode;

    const bool receiverChanged =
        rxconfig.center_freq != oldCenter
        || rxconfig.start_freq != oldStart
        || rxconfig.start_offset_freq != oldOffset;

    if (centerMetaChanged) {
        qInfo() << "[QT5-CENTER-RX]"
                << "center=" << rxconfig.center_freq
                << "samp_rate=" << rxconfig.samp_rate
                << "mode=" << rxconfig.start_mod;
        emit updateCenterFreq();
    }

    if (receiverChanged) {
        const quint64 receiverHz = rxconfig.start_freq > 0
                                 ? rxconfig.start_freq
                                 : static_cast<quint64>(
                                       qMax<qint64>(0,
                                           static_cast<qint64>(rxconfig.center_freq)
                                         + static_cast<qint64>(rxconfig.start_offset_freq)));

        qInfo() << "[QT5-RECEIVER-RX]"
                << "center=" << rxconfig.center_freq
                << "offset=" << rxconfig.start_offset_freq
                << "receiver=" << receiverHz;
        emit receiverStateChanged(rxconfig.center_freq,
                                  rxconfig.start_offset_freq,
                                  receiverHz);
    }

    if (rxconfig.allow_chat)
    {
    }
}

void WebSocketClient::onTextMessageReceived(const QString &message)
{
    if (message.startsWith("CLIENT DE SERVER")) {
        QString paramString = message.mid(17);
        QStringList paramList = paramString.split(' ', Qt::SkipEmptyParts);
        QMap<QString, QString> params;

        for (const QString &param : paramList) {
            QStringList parts = param.split('=');
            if (parts.size() >= 2) {
                QString key = parts[0];
                QString value = parts.mid(1).join('=');
                params[key] = value;
            }
        }

        QString versionInfo = "Unknown server";
        if (params.contains("server") && params["server"] == "openwebrx" && params.contains("version")) {
            versionInfo = "OpenWebRX+ version: " + params["version"];
        }

        qDebug() << ("Server acknowledged WebSocket connection, " + versionInfo);
        return;
    }

    // qDebug() << "onTextMessageReceived :" << message;
    // Try parsing as JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);

    if (!parseError.error && doc.isObject()) {
        QJsonObject obj = doc.object();

        QString type = obj.value("type").toString();
        QJsonValue value = obj.value("value");
        // qDebug() << "onTextMessageReceived" << obj;
        if (type == "config")
        {
            qDebug() << "value:" << value;
            handleConfigMessage(value.toObject());
            if (message.contains("waterfall_colors"))
                emit waterfallColorMap(rxconfig.getWaterfallColorMap());
            else if(message.contains("waterfall_levels"))
                emit waterfallLevelsChanged(rxconfig.waterfall_levels.min,rxconfig.waterfall_levels.max);
        }
        else if (type == "secondary_config") {
            qDebug() << "handleSecondaryConfig" << (value.toObject());
        } else if (type == "receiver_details") {
            qDebug() << "setReceiverDetails" << (value);
        } else if (type == "smeter") {
            const double linearPower = qMax(value.toDouble(), 1e-15);
            emit smeterValueUpdated(10.0 * std::log10(linearPower));
        } else if (type == "cpuusage") {
            // qDebug() << "updateCpuUsage" << (value);
        } else if (type == "temperature") {
            emit onTemperatureChanged(value.toDouble());
            // qDebug() << "updateTemperature" << (value);
        } else if (type == "clients") {
            qDebug() << "updateClientCount" << (value);
        } else if (type == "bands") {
            qDebug() << "bandplan.update" << (value);
        } else if (type == "profiles") {
            qDebug() << "updateProfiles" << (value.toArray());
            updateProfilesValue = value.toArray();
            emit updateProfiles(updateProfilesValue);
        } else if (type == "features") {
            qDebug() << "updateFeatures" << (value.toObject());
        } else if (type == "metadata") {
            // qDebug() << "updateMetadataPanels" << (value.toObject());
        } else if (type == "dial_frequencies") {
            qDebug() << "updateDialFrequencies" << (value.toArray());
        } else if (type == "bookmarks") {
            qDebug() << "bookmarks.replaceBookmarks" << (value.toArray()  <<  "server");
        } else if (type == "sdr_error") {
            const QString error = value.toString();
            qWarning() << "[ASTRARX-ERROR]" << error;
            emit backendError(error);
        } else if (type == "demodulator_error") {
            const QString error = value.toString();
            qWarning() << "[ASTRARX-DEMOD-ERROR]" << error;
            emit backendError(error);
        } else if (type == "secondary_demod") {
            // if (!dispatchSecondaryDemodMessage(value)) {
            qDebug() << " secondary_demod_push_data" <<  (value);
            // }
        } else if (type == "log_message") {
            qDebug() << " divlog(value.toString()" <<   true;
        } else if (type == "chat_message") {
            QJsonObject chat = obj;
            qDebug() << " Chat::recvMessage" <<  (chat["name"].toString(), chat["text"].toString(), chat["color"].toString());
        } else if (type == "backoff") {
            qDebug() << "divlog" <<  "Server is currently busy: " + obj["reason"].toString() << true;
            qDebug() << "showErrorOverlay" << (obj["reason"].toString());
            qDebug() << "reconnect_timeout = 16000";
        } else if (type == "squelch") {
            m_explicitSquelchSeen = true;
            const bool nextSql = value.toBool(false);
            sqlCount = nextSql ? 0 : 3;
            if (sqlOn != nextSql) {
                sqlOn = nextSql;
                qInfo() << "[QT5-SQUELCH-RX]" << sqlOn;
                emit onSQLChanged(sqlOn);
            }
        } else if (type == "modes") {
            qDebug() << "Modes::setModes" << (value.toArray());
        } else {
            qWarning() << "Received unknown message type:" << type << message;
        }
    } else {
        qWarning() << "Invalid JSON message received:" << parseError.errorString();
    }
}
void WebSocketClient::sendDspAction(const QString &action) {
    QJsonObject json;
    json["type"] = "dspcontrol";
    json["action"] = action;

    webSocket.sendTextMessage(QJsonDocument(json).toJson(QJsonDocument::Compact));
}

void WebSocketClient::sendDspControl(int lowCut, int highCut, int offsetFreq,
                                     const QString &mod, int dmrFilter,
                                     int audioServiceId, int squelchLevel,
                                     bool secondaryMod) {
    QJsonObject params;
    params["low_cut"] = lowCut;
    params["high_cut"] = highCut;
    params["offset_freq"] = offsetFreq;
    params["mod"] = mod;
    params["dmr_filter"] = dmrFilter;
    params["audio_service_id"] = audioServiceId;
    params["squelch_level"] = squelchLevel;
    params["secondary_mod"] = secondaryMod;

    QJsonObject json;
    json["type"] = "dspcontrol";
    json["params"] = params;

    webSocket.sendTextMessage(QJsonDocument(json).toJson(QJsonDocument::Compact));
}

void WebSocketClient::sendConnectionProperties(int outputRate, int hdOutputRate)
{
    QJsonObject params;
    params["output_rate"] = outputRate;
    params["hd_output_rate"] = hdOutputRate;

    QJsonObject json;
    json["type"] = "connectionproperties";
    json["params"] = params;

    webSocket.sendTextMessage(QJsonDocument(json).toJson(QJsonDocument::Compact));
}

void WebSocketClient::sendFrequency(quint64 freq) {
    QJsonObject json;
    json["command"] = "set_freq";
    json["freq"] = static_cast<double>(freq);
    webSocket.sendTextMessage(QJsonDocument(json).toJson(QJsonDocument::Compact));
}

// ===================== volumePercent property =====================

Q_INVOKABLE void WebSocketClient::setSpeakerVolumeMute(bool active)
{
    qDebug() << "setSpeakerVolumeMute::" << active;
    if (active) {
        // ----- MUTE -----
        if (!m_isMuted) {
            m_lastVolumeBeforeMute = m_volumePercent;   // จำค่าเดิม
            m_volumePercent = 0;
            m_isMuted = true;
        }
    } else {
        // ----- UNMUTE -----
        if (m_isMuted) {
            m_volumePercent = qBound(0, m_lastVolumeBeforeMute, 100);
            m_isMuted = false;
        }
    }
    emit mutedChanged(m_isMuted);
}

int WebSocketClient::volumePercent() const
{
    bool ok = true;
    int v = m_volumePercent;
    if (!ok)
        return 100;   // fallback ถ้า parse ไม่ได้

    if (v < 0)   v = 0;
    if (v > 100) v = 100;
    return v;
}

void WebSocketClient::setVolumePercent(int percent)
{
    // qDebug() << "SET CALLED setVolumePercent(" << percent << ")";

    int clamped = qBound(0, percent, 100);
    m_volumePercent = clamped;

    emit volumePercentChanged(m_volumePercent);
    // qDebug() << "NOW m_volumePercent =" << m_volumePercent << volumePercent();
}


// ===================== inject ALSA player =====================

void WebSocketClient::setHdAudioPlayer(AlsaAudioPlayer *player)
{
    hdAudioPlayer = player;
}

// ===================== helper: apply volume =====================

// ฟังก์ชันช่วย ปรับระดับเสียงใน buffer PCM 16-bit (signed)
void WebSocketClient::applySoftwareVolume(QByteArray &pcm16)
{
    if (volumePercent() == 100)
        return; // ไม่ต้องยุ่ง ถ้า 100%

    if (pcm16.isEmpty())
        return;

    qint16 *samples = reinterpret_cast<qint16*>(pcm16.data());
    const int sampleCount = pcm16.size() / sizeof(qint16);

    const float gain = volumePercent() / 100.0f;

    for (int i = 0; i < sampleCount; ++i) {
        float s = static_cast<float>(samples[i]) * gain;

        if (s > 32767.0f)
            s = 32767.0f;
        else if (s < -32768.0f)
            s = -32768.0f;

        samples[i] = static_cast<qint16>(s);
    }
}


// ===================== core: ADPCM → PCM → volume → push =====================

void WebSocketClient::processAdpcmAndPlay(const QByteArray &adpcmData)
{
    if (!hdAudioPlayer) {
        qWarning() << "[AudioManager] hdAudioPlayer is null" << hdAudioPlayer;
        return;
    }
    if (adpcmData.isEmpty())
        return;

    // Match the active type-4 ADPCM path: decoder lifetime is local to the
    // synchronized packet. The previous uninitialized raw pointer was undefined
    // behavior and could dereference a random address.
    PCMImaAdpcmCodec decoder;
    QByteArray adpcmChunk = adpcmData;
    QVector<qint16> pcmSamples = decoder.decodeWithSync(adpcmChunk);

    if (pcmSamples.isEmpty())
        return;

    // 3) แปลง QVector<qint16> → QByteArray
    QByteArray pcm(reinterpret_cast<const char*>(pcmSamples.constData()), pcmSamples.size() * sizeof(qint16));

    // 4) push เข้า ALSA
    hdAudioPlayer->pushAudio(pcm);
}

// ===================== playTestTone (optional) =====================

void WebSocketClient::playTestTone()
{
    if (!hdAudioPlayer) {
        qWarning() << "[AudioManager] playTestTone: hdAudioPlayer is null";
        return;
    }

    const int sampleRate = qMax(1, hdAudioPlayer->sampleRate());
    const float freq = 1000.0f;    // 1 kHz
    const int durationMs = 200;    // 0.2 s
    const int totalSamples = sampleRate * durationMs / 1000;

    QVector<qint16> pcmSamples;
    pcmSamples.resize(totalSamples);

    for (int i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float s = qSin(2.0f * M_PI * freq * t);
        pcmSamples[i] = static_cast<qint16>(s * 32767);
    }

    // ใส่ volume
    // applyVolumeToPcm16(pcmSamples, volumePercent());

    QByteArray pcm(
        reinterpret_cast<const char*>(pcmSamples.constData()),
        pcmSamples.size() * static_cast<int>(sizeof(qint16))
        );

    hdAudioPlayer->pushAudio(pcm);
}
