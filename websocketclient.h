#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

// websocketclient.h
#include "alsaaudioplayer.h"
#include "qtimer.h"
#pragma once

#include <QObject>
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>
#include "ImaAdpcmCodec.h"

#include <QAudioFormat>
#include <QAudioOutput>
#include <QBuffer>
#include "pcmImaadpcmcodec.h"   // แก้ชื่อ include ให้ตรงไฟล์จริงของคุณ

#include <QtMath>
#include <QDebug>
#include <QElapsedTimer>
#include <QVector>

typedef QVector<float> Float32BitArray;

class WebSocketClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool muted READ isMute NOTIFY mutedChanged)
    Q_PROPERTY(bool fftUiActive READ fftUiActive WRITE setFftUiActive NOTIFY fftUiActiveChanged)
    Q_PROPERTY(bool maxHoldEnabled READ maxHoldEnabled WRITE setMaxHoldEnabled NOTIFY maxHoldEnabledChanged)
    // R20.4: native FFT auto-scale statistics. QML reads only two scalars;
    // no full FFT JavaScript array is required in the production renderer.
    Q_PROPERTY(double fftNoiseDb READ fftNoiseDb NOTIFY fftAutoScaleStatsChanged)
    Q_PROPERTY(double fftStrongDb READ fftStrongDb NOTIFY fftAutoScaleStatsChanged)
    Q_PROPERTY(bool fftAutoScaleValid READ fftAutoScaleValid NOTIFY fftAutoScaleStatsChanged)
public:
    explicit WebSocketClient(QObject *parent = nullptr);
    ~WebSocketClient() override;

    int  m_volumePercent = 25;   // 0–100
    int m_lastVolumeBeforeMute = 100;
    bool m_isMuted = false;


    // ===== property สำหรับ QML =====
    Q_INVOKABLE int isMute() const { return m_isMuted; }

    // FFT UI runtime gate. Audio and the WebSocket connection remain active.
    bool fftUiActive() const noexcept { return m_fftUiActive; }
    Q_INVOKABLE void setFftUiActive(bool active);

    // Native max-hold engine. Keeping the O(N) accumulation loop in C++ avoids
    // running a full FFT-sized JavaScript loop inside Canvas::onPaint.
    bool maxHoldEnabled() const noexcept { return m_maxHoldEnabled; }
    Q_INVOKABLE void setMaxHoldEnabled(bool enabled);
    Q_INVOKABLE void resetMaxHold();
    Q_INVOKABLE QVariantList maxHoldSnapshot() const;
    double fftNoiseDb() const noexcept { return m_fftNoiseDb; }
    double fftStrongDb() const noexcept { return m_fftStrongDb; }
    bool fftAutoScaleValid() const noexcept { return m_fftAutoScaleValid; }

    Q_INVOKABLE int  volumePercent() const;           // 0–100
    Q_INVOKABLE void setVolumePercent(int percent);   // 0–100

    // ===== ใช้ inject player จาก C++ ฝั่ง main/windows =====
    void setHdAudioPlayer(AlsaAudioPlayer *player);

    // ===== เรียกจาก C++: เอา ADPCM มาถอด, ใส่ volume, push เข้า ALSA =====
    Q_INVOKABLE void processAdpcmAndPlay(const QByteArray &adpcmData);

    // test tone (ถ้าอยากลองแบบไม่ต้องใช้ ADPCM)
    Q_INVOKABLE void playTestTone();


    Q_INVOKABLE void setSpeakerVolumeMute(bool active);

    void connectToServer(const QUrl &url);
    static constexpr int COMPRESS_FFT_PAD_N = 10;
    struct rxwsConfig {
        // ---- Audio & Chat Options ----
        bool allow_audio_recording = false;
        bool allow_center_freq_changes = false;
        bool allow_chat = false;
        QString audio_compression = "none";
        QString fft_compression = "none";

        // ---- Spectrum / FFT ----
        int fft_size = 0;
        int tuning_precision = 0;
        int tuning_step_default = 0;
        int max_clients = 0;
        int samp_rate = 0;
        quint64 center_freq = 0;
        quint64 start_freq = 0;
        int start_offset_freq = 0;
        QString start_mod;
        QString sdr_id;
        QString profile_id;

        // ---- Squelch & Scanner ----
        int squelch_auto_margin = 0;

        // ---- Receiver Position ----
        struct {
            double lat = 0.0;
            double lon = 0.0;
        } receiver_gps;

        // ---- Waterfall Settings ----
        struct {
            int min = -100;
            int max = 0;
        } waterfall_levels;

        struct {
            int min = 0;
            int max = 0;
        } waterfall_auto_levels;

        int waterfall_auto_min_range = 0;
        bool waterfall_auto_level_default_mode = true;
        QString waterfall_scheme;
        QVector<int> waterfall_colors;

        // ---- External URLs ----
        QString callsign_url;
        QString flight_url;
        QString modes_url;
        QString vessel_url;

        // ---- JSON Deserialization ----
        void fromJson(const QJsonObject &json) {
            auto tryGet = [&](const QString &key, auto &dest) {
                if (json.contains(key)) dest = json.value(key).toVariant().value<std::decay_t<decltype(dest)>>();
            };

            tryGet("allow_audio_recording", allow_audio_recording);
            tryGet("allow_center_freq_changes", allow_center_freq_changes);
            tryGet("allow_chat", allow_chat);
            tryGet("audio_compression", audio_compression);
            tryGet("fft_compression", fft_compression);
            tryGet("fft_size", fft_size);
            tryGet("tuning_precision", tuning_precision);
            tryGet("tuning_step", tuning_step_default);
            tryGet("max_clients", max_clients);
            tryGet("samp_rate", samp_rate);
            tryGet("center_freq", center_freq);
            tryGet("start_freq", start_freq);
            tryGet("start_offset_freq", start_offset_freq);
            tryGet("start_mod", start_mod);
            tryGet("sdr_id", sdr_id);
            tryGet("profile_id", profile_id);
            tryGet("squelch_auto_margin", squelch_auto_margin);
            tryGet("waterfall_auto_min_range", waterfall_auto_min_range);
            tryGet("waterfall_auto_level_default_mode", waterfall_auto_level_default_mode);
            tryGet("waterfall_scheme", waterfall_scheme);
            tryGet("callsign_url", callsign_url);
            tryGet("flight_url", flight_url);
            tryGet("modes_url", modes_url);
            tryGet("vessel_url", vessel_url);

            if (json.contains("receiver_gps")) {
                QJsonObject gps = json["receiver_gps"].toObject();
                receiver_gps.lat = gps.value("lat").toDouble(receiver_gps.lat);
                receiver_gps.lon = gps.value("lon").toDouble(receiver_gps.lon);
            }

            if (json.contains("waterfall_levels")) {
                QJsonObject wf = json["waterfall_levels"].toObject();
                waterfall_levels.min = wf["min"].toInt(waterfall_levels.min);
                waterfall_levels.max = wf["max"].toInt(waterfall_levels.max);
            }

            if (json.contains("waterfall_auto_levels")) {
                QJsonObject wf = json["waterfall_auto_levels"].toObject();
                waterfall_auto_levels.min = wf["min"].toInt(waterfall_auto_levels.min);
                waterfall_auto_levels.max = wf["max"].toInt(waterfall_auto_levels.max);
            }

            if (json.contains("waterfall_colors")) {
                waterfall_colors.clear();
                for (const QJsonValue &val : json["waterfall_colors"].toArray()) {
                    waterfall_colors.append(val.toInt());
                }
            }
        }

        QVariantList getWaterfallColorMap() const {
            QVariantList list;
            for (int color : waterfall_colors) list.append(color);
            return list;
        }
    };


    rxwsConfig rxconfig;
    QWebSocket webSocket;
    QJsonArray updateProfilesValue;
    QTimer resetSQL;

signals:
    void mutedChanged(bool muted);
    void fftUiActiveChanged(bool active);
    void maxHoldEnabledChanged(bool enabled);
    void maxHoldUpdated(QVariantList maxHoldData);

    // R20.4 production renderer signals. These stay entirely in C++ and use
    // implicitly-shared QVector<float>; no QVariantList/QV4 boxing is required.
    void spectrumDisplayFrame(QVector<float> fftData);
    void waterfallDisplayFrame(QVector<float> fftData);
    void maxHoldDisplayFrame(QVector<float> fftData);
    void fftAutoScaleStatsChanged();

    // Primary full-span FFT frame. Spectrum and Waterfall consume this single
    // delivery in QML to avoid crossing the C++/QML boundary twice per frame.
    void fftFrameUpdated(QVariantList fftData);
    // Legacy compatibility signals are intentionally retained in the public
    // API, but the optimized primary FFT path emits fftFrameUpdated() only.
    void spectrumUpdated(QVariantList spectrumData);
    void waterfallUpdated(QVariantList spectrumData);
    void smeterValueUpdated(double smeterValue);
    void waterfallColorMap(QVariantList waterfall_colors);
    void waterfallLevelsChanged(int min, int max);
    // Center/source metadata changed (center/sample-rate/mode). Kept separate
    // from receiver offset updates so DSP tuning never masquerades as an RF retune.
    void updateCenterFreq();
    // Atomic receiver snapshot from the server.
    void receiverStateChanged(quint64 centerHz, int offsetHz, quint64 receiverHz);
    void backendError(QString message);
    void updateProfiles(QJsonArray value);

    void openwebrxConnected();
    void onSQLChanged(bool sqlVal);
    void onTemperatureChanged(double temp);
    void volumePercentChanged(int newVolume);

private:
    bool m_fftUiActive = false;

    // AstraRX connection recovery state. The UI-facing WebSocket contract stays
    // unchanged; reconnect is entirely backend-owned.
    QTimer m_reconnectTimer;
    QUrl m_targetUrl;
    int m_reconnectDelayMs = 1000;
    bool m_shuttingDown = false;
    void scheduleReconnect(const QString &reason);
    void openBackendSocket();

    // R20.4 native FFT transport. Incoming frames are validated once in C++.
    // Spectrum/Waterfall receive QVector<float> directly, while the legacy
    // QVariantList bridge is opt-in only for diagnostics.
    bool m_fftQmlPublish = false;
    bool m_fftStrictSize = true;
    int m_fftMaxBins = 65536;
    int m_spectrumDisplayBins = 8192;
    int m_waterfallDisplayBins = 2048;

    QElapsedTimer m_fftUiPublishTimer;
    int m_fftUiPublishIntervalMs = 40;       // legacy QML bridge: 25 Hz
    QElapsedTimer m_spectrumDisplayTimer;
    int m_spectrumDisplayIntervalMs = 16;    // native spectrum: target up to ~60 Hz
    QElapsedTimer m_waterfallDisplayTimer;
    int m_waterfallDisplayIntervalMs = 16;   // native waterfall: target up to ~60 Hz

    // FPS telemetry is diagnostic-only and bounded. It lets us distinguish
    // AstraRX source cadence from Qt/GPU render cadence on the target.
    QElapsedTimer m_fftSourceStatsTimer;
    quint64 m_fftSourceStatsFrames = 0;
    QElapsedTimer m_fftAutoScaleTimer;
    int m_fftAutoScaleIntervalMs = 500;
    double m_fftNoiseDb = -120.0;
    double m_fftStrongDb = -80.0;
    bool m_fftAutoScaleValid = false;
    QVector<float> m_fftAutoScaleScratch;

    bool m_maxHoldEnabled = false;
    QVector<float> m_maxHold;
    QElapsedTimer m_maxHoldPublishTimer;
    int m_maxHoldPublishIntervalMs = 200; // 5 Hz display snapshots

    // Reused full-resolution decode buffer.
    QVector<float> m_fftDecodeScratch;

    bool shouldPublishFftUiFrame();
    bool shouldPublishSpectrumFrame();
    bool shouldPublishWaterfallFrame();
    bool validateFftFrame(const QVector<float> &fftFrame, int payloadBytes = -1);
    QVector<float> peakPoolForDisplay(const QVector<float> &source, int limit) const;
    void publishNativeFftFrames(const QVector<float> &fftFrame);
    void updateFftAutoScaleStats(const QVector<float> &fftFrame);
    QVariantList fftToVariantList(const QVector<float> &fftFrame) const;

    void prepareMaxHold(int count);
    void updateMaxHoldValue(int index, float value);
    void updateMaxHold(const QVector<float> &fftFrame);
    void publishMaxHoldIfDue();
    QVariantList maxHoldToVariantList() const;

    ImaAdpcmCodec fft_codec;
    ImaAdpcmCodec pcm_codec;

    void handleConfigMessage(const QJsonObject &config);
    AlsaAudioPlayer *hdAudioPlayer = new AlsaAudioPlayer(16000, SND_PCM_FORMAT_S16_LE); // no 'this' parent
    AlsaAudioPlayer *sdAudioPlayer = new AlsaAudioPlayer(12000, SND_PCM_FORMAT_S16_LE); // no 'this' parent
    int sqlCount = 0;
    bool sqlOn = false;
    bool m_explicitSquelchSeen = false;
    void resetSQLCount();
    void applyVolumeToPcm16(QVector<qint16> &samples, int volumePercent);
    void applySoftwareVolume(QByteArray &pcm16);

public slots:
    void sendFrequency(quint64 freq);
    void sendConnectionProperties(int outputRate, int hdOutputRate);
    void sendDspControl(int lowCut, int highCut, int offsetFreq, const QString &mod, int dmrFilter, int audioServiceId, int squelchLevel, bool secondaryMod);

private slots:
    void onConnected();
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void attemptReconnect();
    void onBinaryMessageReceived(const QByteArray &message);
    void onTextMessageReceived(const QString &message);
    void sendDspAction(const QString &action);
};


#endif // WEBSOCKETCLIENT_H
