// websocketclient.cpp
#include "websocketclient.h"
#include "pcmImaadpcmcodec.h"
#include <QDebug>
#include <cmath>
WebSocketClient::WebSocketClient(QObject *parent) : QObject(parent) {}

void WebSocketClient::connectToServer(const QUrl &url) {
    connect(&webSocket, &QWebSocket::connected, this, &WebSocketClient::onConnected);
    connect(&webSocket, &QWebSocket::textMessageReceived, this, &WebSocketClient::onTextMessageReceived);
    connect(&webSocket, &QWebSocket::binaryMessageReceived, this, &WebSocketClient::onBinaryMessageReceived);
    connect(&resetSQL, &QTimer::timeout, this, &WebSocketClient::resetSQLCount);
    resetSQL.start(100);
    webSocket.open(url);
}

void WebSocketClient::onConnected() {
    qDebug() << "Connected to OpenWebRX.";
    webSocket.sendTextMessage("SERVER DE CLIENT client=openwebrx.js type=receiver");
    sendConnectionProperties(12000, 16000);
    // sendDspControl(-75000, 75000, 0, "wfm", 3, -1250000, -150, false);
    sendDspAction("start") ;

    hdAudioPlayer->start();
    sdAudioPlayer->start();
    qDebug() << "address hdAudioPlayer::s" << hdAudioPlayer;
    emit openwebrxConnected();
}
void WebSocketClient::resetSQLCount()
{
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
    if (message.isEmpty()) return;

    // qDebug() << "onBinaryMessageReceived" << message;
    quint8 type = static_cast<quint8>(message.at(0));
    QByteArray data = message.mid(1);

    // Track network speed if needed:
    // networkSpeedMeasurement.add(message.size());

    QVector<float> waterfallF32;

    switch (type)
    {
    case 1: { // FFT data
        // qDebug() << "rxconfig.fft_compression " << rxconfig.fft_compression ;
        if (rxconfig.fft_compression == "none")
        {
            int sampleCount = data.size() / sizeof(float);
            const float* floatData = reinterpret_cast<const float*>(data.constData());
            waterfallF32 = QVector<float>(floatData, floatData + sampleCount);

        }
        else if (rxconfig.fft_compression == "adpcm") {
            fft_codec.reset();

            // Decode from ADPCM to int16_t
            QVector<qint16> waterfallI16 = fft_codec.decode(QByteArray(data));  // fft_codec should implement decode(QByteArray)

            // qDebug() << "Decode from ADPCM to int16_t" << waterfallI16;
            int count = waterfallI16.size() - COMPRESS_FFT_PAD_N;
            if (count > 0) {
                waterfallF32.resize(count);
                for (int i = 0; i < count; ++i)
                    waterfallF32[i] = static_cast<float>(waterfallI16[i + COMPRESS_FFT_PAD_N]) / 100.0f;
            }
        }

        QVariantList list;
        list.reserve(waterfallF32.size());

        for (float f : waterfallF32) {
            list.append(f);
        }
        // qDebug() << "spectrumUpdated list" << list;
        emit spectrumUpdated(list);
        emit waterfallUpdated(list);

        // QVariantList line;
        // for (float val : waterfallF32)
        //     line.append(val);
        // emit waterfallUpdated(line);

        // waterfall_add(waterfallF32);
        // spectrum.update(waterfallF32);
        // scanner.update(waterfallF32);
        // monitorLevels(waterfallF32);
        break;
    }

    case 2: { // audio data
        if (rxconfig.audio_compression == "none") {
            // const qint16* raw = reinterpret_cast<const qint16*>(data.constData());
            // qDebug() << "audio data rxconfig.audio_compression";
            int sampleCount = data.size() / sizeof(qint16);
            if (sampleCount > 0) {

                applySoftwareVolume(data);

                if (sdAudioPlayer) {
                    // qDebug() << "[WebSocketClient] sdAudioPlayer is success";
                    sdAudioPlayer->pushAudio(data);
                } else {
                    // qWarning() << "[WebSocketClient] sdAudioPlayer is null";
                }
            } else {
                qWarning() << "Invalid PCM size:" << data.size();
            }
            sqlCount = 0;
            if(sqlOn != true){
                sqlOn = true;
                emit onSQLChanged(sqlOn);
                qDebug() << "sd audio data" << "data" << data.size() << "rxconfig.audio_compression" << rxconfig.audio_compression;
            }

        }
        break;
    }

    case 3: { // secondary FFT
        QVector<float> secondaryF32;

        if (rxconfig.fft_compression == "none") {
            int sampleCount = data.size() / sizeof(float);
            const float* floatData = reinterpret_cast<const float*>(data.constData());
            secondaryF32 = QVector<float>(floatData, floatData + sampleCount);
        } else if (rxconfig.fft_compression == "adpcm") {
            fft_codec.reset();
            QVector<qint16> waterfallI16 = fft_codec.decode(QByteArray(data));
            int count = waterfallI16.size() - COMPRESS_FFT_PAD_N;
            if (count > 0) {
                secondaryF32.resize(count);
                for (int i = 0; i < count; ++i)
                    secondaryF32[i] = static_cast<float>(waterfallI16[i + COMPRESS_FFT_PAD_N]) / 100.0f;
            }
        }

        // secondary_demod_waterfall_add(secondaryF32);
        break;
    }

    case 4: { // hd audio data
        if (rxconfig.audio_compression == "none")
        {
            int sampleCount = data.size() / sizeof(qint16);
            if (sampleCount > 0) {
                // ปรับเสียงบน buffer data โดยตรง
                applySoftwareVolume(data);
                // qDebug() << "m_volumePercent" << m_volumePercent;

                if (hdAudioPlayer) {
                    // qDebug() << "[WebSocketClient] hdAudioPlayer is success";
                    hdAudioPlayer->pushAudio(data);
                } else {
                    // qWarning() << "[WebSocketClient] hdAudioPlayer is null";
                }
            } else {
                qWarning() << "Invalid PCM size:" << data.size();
            }
        }
        else if (rxconfig.audio_compression == "adpcm")
        {
            PCMImaAdpcmCodec decoder;
            QByteArray adpcmChunk = data; // from WebSocket

            QVector<qint16> pcmSamples = decoder.decodeWithSync(adpcmChunk);

            QByteArray pcm(reinterpret_cast<const char*>(pcmSamples.constData()),
                           pcmSamples.size() * sizeof(qint16));

            // ★ ปรับ volume หลัง decode แล้ว
            applySoftwareVolume(pcm);
            // qDebug() << "m_volumePercent" << m_volumePercent;

            if (hdAudioPlayer) {
                hdAudioPlayer->pushAudio(pcm);
            } else {
                // qWarning() << "[WebSocketClient] hdAudioPlayer is null";
            }
        }

        sqlCount = 0;
        if (!sqlOn) {
            sqlOn = true;
            qDebug() << "hd audio data" << "data" << data.size()
                     << "rxconfig.audio_compression" << rxconfig.audio_compression;
            emit onSQLChanged(sqlOn);
        }

        break;
    }


    default:
        qWarning() << "Unknown binary message type:" << type;
        break;
    }
}
// void WebSocketClient::onBinaryMessageReceived(const QByteArray &message)
// {
//     if (message.isEmpty()) return;

//     quint8 type = static_cast<quint8>(message.at(0));
//     QByteArray data = message.mid(1);

//     switch (type)
//     {
//         case 1: // FFT data
//             QVariantList spectrum;
//             const int floatSize = sizeof(float);
//             int count = message.size() / floatSize;

//             for (int i = 0; i < count; ++i)
//             {
//                 float value;
//                 memcpy(&value, message.constData() + i * floatSize, floatSize);
//                 if (!std::isnan(value))
//                     spectrum.append(value);
//             }

//             // qDebug() << "spectrumUpdated" << spectrum;
//             emit spectrumUpdated(spectrum);
//         break;
//     }
// }
void WebSocketClient::handleConfigMessage(const QJsonObject &config)
{
    rxconfig.fromJson(config);

    if (rxconfig.allow_chat )
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
            else if(message.contains("start_offset_freq"))
                emit updateCenterFreq();
        }
        else if (type == "secondary_config") {
            qDebug() << "handleSecondaryConfig" << (value.toObject());
        } else if (type == "receiver_details") {
            qDebug() << "setReceiverDetails" << (value);
        } else if (type == "smeter") {
            emit smeterValueUpdated(10*(std::log10(value.toDouble())));
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
            qDebug() << "divlog(value.toString()" <<  true;
            qDebug() << "showErrorOverlay" << (value.toString());
            qDebug() << "stopDemodulator()";
        } else if (type == "demodulator_error") {
            qDebug() << "divlog" << value.toString() << true;
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

void WebSocketClient::sendFrequency(int freq) {
    QJsonObject json;
    json["command"] = "set_freq";
    json["freq"] = freq;
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
    PCMImaAdpcmCodec *m_decoder;
    if (!hdAudioPlayer) {
        qWarning() << "[AudioManager] hdAudioPlayer is null" << hdAudioPlayer;
        return;
    }
    if (!m_decoder) {
        qWarning() << "[AudioManager] decoder is null" << m_decoder;
        return;
    }
    if (adpcmData.isEmpty())
        return;

    // 1) decode ADPCM → PCM16
    QByteArray adpcmChunk = adpcmData; // from WebSocket
    QVector<qint16> pcmSamples = m_decoder->decodeWithSync(adpcmChunk);

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

    const int sampleRate = 48000;
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
