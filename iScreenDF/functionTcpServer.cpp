#include "iScreenDF.h"

// ============================================================================
// DB helper
// ใช้สำหรับส่งงาน DB ให้ไปรันใน thread ของ DatabaseDF เท่านั้น
// ห้ามเรียก db->UpdateParameterField(...) ตรง ๆ จาก iScreenDF thread
// ============================================================================
static void queueUpdateParameterField(DatabaseDF *db,
                                      const QString &field,
                                      const QVariant &value)
{
    if (!db) {
        qWarning() << "[queueUpdateParameterField] db is null, field =" << field;
        return;
    }

    QTimer::singleShot(0, db, [db = db, field, value]() {
        db->UpdateParameterField(field, value);
    });
}

void iScreenDF::sendParameterToServer()
{
    applyRfsocParameterToServer(true);
}

void iScreenDF::sendRfsocJsonLine(const QJsonObject &obj, bool addNewline)
{
    if (!localDFclient) {
        qWarning() << "[iScreenDF][RFSoC] localDFclient is null, drop:" << obj;
        return;
    }

    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    localDFclient->sendLine(payload, addNewline);
}

static int maskToPrefix(const QString &mask)
{
    const auto parts = mask.split('.', Qt::SkipEmptyParts);
    if (parts.size() != 4)
        return -1;

    int prefix = 0;
    bool zeroSeen = false;

    for (const QString &p : parts) {
        bool ok = false;
        int oct = p.toInt(&ok);
        if (!ok || oct < 0 || oct > 255)
            return -1;

        for (int b = 7; b >= 0; --b) {
            const bool bit = (oct >> b) & 1;
            if (bit) {
                if (zeroSeen)
                    return -1;
                prefix++;
            } else {
                zeroSeen = true;
            }
        }
    }

    return prefix;
}

static QString makeIpCidr(const QString &ip, const QString &mask)
{
    const int pfx = maskToPrefix(mask);
    if (pfx <= 0)
        return ip;

    return ip + "/" + QString::number(pfx);
}

static QString makeDnsString(const QString &dns1, const QString &dns2)
{
    QStringList list;

    if (!dns1.trimmed().isEmpty())
        list << dns1.trimmed();

    if (!dns2.trimmed().isEmpty())
        list << dns2.trimmed();

    return list.join(",");
}

static QString makeModeFromDhcp(const QString &dhcp)
{
    const QString v = dhcp.trimmed().toLower();

    if (v == "1" || v == "true" || v == "dhcp" || v == "on" || v == "enable")
        return "dhcp";

    return "static";
}

void iScreenDF::requestRfFrequency()
{
    if (m_parameter.isEmpty() || !m_parameter.first())
        return;

    Parameter *p = m_parameter.first();
    emit rfsocParameterUpdated(p->m_Frequency, p->m_doaBwHz);
    emit updateDoaLineMeters(p->m_maxDoaLine_meters);
}

void iScreenDF::onUpdateNetworkDfDevice(const QString &iface,
                                        const QString &dhcp,
                                        const QString &ip,
                                        const QString &mask,
                                        const QString &gw,
                                        const QString &dns1,
                                        const QString &dns2)
{
    QJsonObject obj;

    obj["menuID"]  = "setIpConfig";
    obj["ifname"]  = iface;
    obj["ip"]      = ip;
    obj["netmask"] = mask;
    obj["gateway"] = gw;
    obj["dns1"]    = dns1;
    obj["dns2"]    = dns2;

    QJsonDocument doc(obj);
    qDebug().noquote()
        << "[iScreenDF][setIpConfig][JSON] ="
        << doc.toJson(QJsonDocument::Compact);

    sendRfsocJsonLine(obj, true);
}

void iScreenDF::GetrfsocParameter(bool setDoaEnable,
                                  bool spectrumEnabled,
                                  int setAdcChannel,
                                  int Frequency,
                                  int update_en,
                                  double TxHz,
                                  int TargetOffsetHz,
                                  int DoaBwHz,
                                  double DoaPowerThresholdDb,
                                  const QString &DoaAlgorithm,
                                  double ucaRadiusM,
                                  double TargetDb,
                                  bool rfAgcEnabled,
                                  bool linkStatus,
                                  double offsetvalue,
                                  double compassoffset,
                                  int maxDoaLineMeters,
                                  const QString &ipLocalForRemoteGroup,
                                  int setDelayMs,
                                  int setDistance)
{
    qDeleteAll(m_parameter);
    m_parameter.clear();

    Parameter *p = new Parameter();
    p->m_setDoaEnable          = setDoaEnable;
    p->m_spectrumEnabled       = spectrumEnabled;
    p->m_setAdcChannel         = setAdcChannel;
    p->m_Frequency             = Frequency;
    p->m_update_en             = update_en;
    p->m_txHz                  = TxHz;
    p->m_TargetOffsetHz        = TargetOffsetHz;
    p->m_doaBwHz               = DoaBwHz;
    p->m_doaPowerThresholdDb   = static_cast<float>(DoaPowerThresholdDb);
    p->m_doaAlgorithm          = DoaAlgorithm;
    p->m_ucaRadiusM            = ucaRadiusM;
    p->m_offset_value          = offsetvalue;
    p->m_compass_offset        = compassoffset;
    p->m_maxDoaLine_meters     = maxDoaLineMeters;
    p->m_ipLocalForRemoteGroup = ipLocalForRemoteGroup;
    p->m_rfAgcEnabled          = rfAgcEnabled;
    p->m_linkStatus            = linkStatus;

    for (int i = 0; i < 5; ++i)
        p->m_rfAgcChEnabled[i] = p->m_rfAgcEnabled;

    auto clampRfAgcDbLocal = [](double v) -> double {
        if (v < -90.0)
            v = -90.0;
        if (v > -30.0)
            v = -30.0;
        return v;
    };

    TargetDb = clampRfAgcDbLocal(TargetDb);

    for (int i = 0; i < 5; ++i)
        p->m_rfAgcTargetDb[i] = TargetDb;

    qDebug() << "[iScreenDF] init rf_agc_enabled =" << p->m_rfAgcEnabled
             << "rf_agc_target_db(all)=" << TargetDb;

    emit updateRfAgcEnableFromServer(-1, p->m_rfAgcEnabled);

    for (int ch = 0; ch < 5; ++ch) {
        qDebug() << "[iScreenDF] init rf_agc_target_db ch=" << ch
                 << "db=" << p->m_rfAgcTargetDb[ch];
        emit updateRfAgcTargetFromServer(ch, p->m_rfAgcTargetDb[ch]);
    }

    m_parameter.append(p);

    emit rfsocParameterUpdated(p->m_Frequency, p->m_doaBwHz);
    emit updateGateThDbFromServer(p->m_doaPowerThresholdDb);
    emit updateTxHzFromServer(p->m_txHz);
    emit rfsocDoaFftUpdated(p->m_setDoaEnable, p->m_spectrumEnabled);
    emit updateDoaAlgorithmFromServer(p->m_doaAlgorithm);
    emit updateUcaRadiusFromServer(p->m_ucaRadiusM);
    emit updatelinkStatus(p->m_linkStatus);
    emit updateGlobalOffsets(p->m_offset_value, p->m_compass_offset);
    emit updateDoaLineMeters(p->m_maxDoaLine_meters);
    emit updateIPLocalForRemoteGroupFromServer(p->m_ipLocalForRemoteGroup);
    emit mapOfflineChanged(true);
    emit updateMaxDoaDelayMsFromServer(setDelayMs);
    emit updateDoaLineDistanceMFromServer(setDistance);

    qDebug() << "[iScreenDF] GetrfsocParameter stored:"
             << "DoA=" << p->m_setDoaEnable
             << "Spec=" << p->m_spectrumEnabled
             << "ADC=" << p->m_setAdcChannel
             << "Freq=" << p->m_Frequency
             << "update_en=" << p->m_update_en
             << "TxHz=" << p->m_txHz
             << "Offset=" << p->m_TargetOffsetHz
             << "BW=" << p->m_doaBwHz
             << "TH=" << p->m_doaPowerThresholdDb;
}

void iScreenDF::setIPLocalForRemoteGroup(const QString &ip)
{
    if (m_parameter.isEmpty() || !m_parameter.first())
        return;

    Parameter *p = m_parameter.first();
    qDebug() << "[iScreenDF] setIPLocalForRemoteGroup =" << ip;

    p->m_ipLocalForRemoteGroup = ip;
    queueUpdateParameterField(db, "IPLocalForRemoteGroup", ip);

    emit updateIPLocalForRemoteGroupFromServer(p->m_ipLocalForRemoteGroup);
}

void iScreenDF::sendMaxDoaLineMeters(int meters)
{
    if (m_parameter.isEmpty() || !m_parameter.first())
        return;

    Parameter *p = m_parameter.first();
    qDebug() << "[iScreenDF] sendMaxDoaLineMeters =" << meters;

    p->m_maxDoaLine_meters = meters;
    queueUpdateParameterField(db, "maxDoaLineMeters", meters);

    emit updateDoaLineMeters(p->m_maxDoaLine_meters);
}

void iScreenDF::setCompassOffset(double offset)
{
    if (m_parameter.isEmpty() || !m_parameter.first())
        return;

    Parameter *p = m_parameter.first();
    p->m_compass_offset = offset;

    qDebug() << "[Compass] setCompassOffset =" << offset;

    queueUpdateParameterField(db, "compass_offset", p->m_compass_offset);

    emit updateGlobalOffsets(p->m_offset_value, p->m_compass_offset);
}

void iScreenDF::GetIPDFServer(const QString &ip)
{
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] GetIPDFServer: no parameter";
        return;
    }

    Parameter *p = m_parameter.first();
    p->m_ipdfServer = ip;

    localDFclient->connectToServer(p->m_ipdfServer, 5555);
    emit updateServeripDfserver(p->m_ipdfServer);

    if (gpsReader) {
        gpsReader->setGpsdEndpoint(p->m_ipdfServer, 2947);
        gpsReader->start();
    }
}

void iScreenDF::updateIPServerDF()
{
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] updateIPServerDF: no parameter";
        return;
    }

    Parameter *p = m_parameter.first();
    qDebug() << "[iScreenDF] ServeripDfserver" << p->m_ipdfServer;

    emit updateServeripDfserver(p->m_ipdfServer);
}

void iScreenDF::connectToDFserver(const QString &ip)
{
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] connectToDFserver: no parameter ip:" << ip;
        return;
    }

    Parameter *p = m_parameter.first();
    p->m_ipdfServer = ip;

    queueUpdateParameterField(db, "ipdfserver", p->m_ipdfServer);

    qDebug() << "[iScreenDF] connectToDFserver" << p->m_ipdfServer;
    localDFclient->connectToServer(p->m_ipdfServer, 5555);

    if (gpsReader) {
        gpsReader->setGpsdEndpoint(p->m_ipdfServer, 2947);
        gpsReader->start();
    }
}

void iScreenDF::applyRfsocParameterToServer(bool needAck)
{
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] applyRfsocParameterToServer: no parameter";
        return;
    }

    const Parameter *p = m_parameter.first();

    {
        QJsonObject o;
        o["menuID"]  = "setDoaEnable";
        o["enable"]  = p->m_setDoaEnable;
        o["needAck"] = needAck;
        sendRfsocJsonLine(o, true);
    }

    {
        QJsonObject o;
        o["menuID"]  = "setSpectrumEnable";
        o["enable"]  = p->m_spectrumEnabled;
        o["needAck"] = needAck;
        sendRfsocJsonLine(o, true);
    }

    {
        QJsonObject o;
        o["menuID"]  = "setAdcChannel";
        o["channel"] = p->m_setAdcChannel;
        o["needAck"] = needAck;
        sendRfsocJsonLine(o, true);
    }

    {
        QJsonObject o;
        o["menuID"]    = "setFrequencyHz";
        o["freq_hz"]   = p->m_Frequency;
        o["update_en"] = p->m_update_en;
        o["needAck"]   = needAck;
        sendRfsocJsonLine(o, true);
    }

    {
        QJsonObject o;
        o["menuID"]  = "setTxHz";
        o["hz"]      = p->m_txHz;
        o["needAck"] = needAck;
        sendRfsocJsonLine(o, true);
    }

    {
        QJsonObject o;
        o["menuID"]    = "setDoaTargetOffsetHz";
        o["offset_hz"] = p->m_TargetOffsetHz;
        o["needAck"]   = needAck;
        sendRfsocJsonLine(o, true);
    }

    {
        QJsonObject o;
        o["menuID"]  = "setDoaBwHz";
        o["bw_hz"]   = p->m_doaBwHz;
        o["needAck"] = needAck;
        sendRfsocJsonLine(o, true);
    }

    {
        QJsonObject o;
        o["menuID"]  = "setDoaPowerThresholdDb";
        o["th_db"]   = static_cast<double>(p->m_doaPowerThresholdDb);
        o["needAck"] = needAck;
        sendRfsocJsonLine(o, true);
    }

    {
        QJsonObject o;
        o["menuID"]   = "setUcaRadiusM";
        o["radius_m"] = p->m_ucaRadiusM;
        o["needAck"]  = needAck;
        sendRfsocJsonLine(o, true);
    }

    {
        QJsonObject o;
        o["menuID"]  = "setDoaAlgorithm";
        o["algo"]    = p->m_doaAlgorithm;
        o["needAck"] = needAck;
        sendRfsocJsonLine(o, true);
    }

    {
        QJsonObject o;
        o["menuID"]  = "setRfAgcEnable";
        o["enable"]  = p->m_rfAgcEnabled;
        o["ch"]      = -1;
        o["needAck"] = true;
        sendRfsocJsonLine(o, true);
    }

    {
        QJsonObject o;
        o["menuID"]  = "getState";
        o["needAck"] = true;
        sendRfsocJsonLine(o, true);
    }

    for (int ch = 0; ch < 5; ++ch) {
        QJsonObject o;
        o["menuID"]    = "setRfAgcChannel";
        o["ch"]        = ch;
        o["target_db"] = p->m_rfAgcTargetDb[ch];
        o["needAck"]   = needAck;
        sendRfsocJsonLine(o, true);

        qDebug() << "[iScreenDF] apply -> setRfAgcChannel ch=" << ch
                 << "db=" << p->m_rfAgcTargetDb[ch];
    }
}

void iScreenDF::onTcpMessage(const QString &message,
                             const QHostAddress &addr,
                             quint16 port)
{
    qDebug() << "[iScreenDF][TCP] message from"
             << addr.toString() << ":" << port
             << "msg =" << message;

    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] onTcpMessage: no parameter";
        return;
    }

    Parameter *p = m_parameter.first();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &err);

    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        QJsonObject obj = doc.object();
        const QString menuID = obj.value("menuID").toString();

        if (menuID == "getStatus") {
            QJsonObject reply;
            reply["menuID"] = "statusReply";
            reply["msg"]    = "OK from iScreenDF";
            reply["ip"]     = p->m_ipLocalForRemoteGroup;

            const QString jsonReply = QString::fromUtf8(
                QJsonDocument(reply).toJson(QJsonDocument::Compact));

            tcpServerDF->broadcastLine(jsonReply);
        }
        else if (menuID == "getName") {
            QJsonObject reply;
            reply["menuID"] = "getName";
            reply["name"]   = controllerName;
            reply["serial"] = Serialnumber;
            reply["ip"]     = p->m_ipLocalForRemoteGroup;

            const QString jsonReply = QString::fromUtf8(
                QJsonDocument(reply).toJson(QJsonDocument::Compact));

            tcpServerDF->broadcastLine(jsonReply);
        }
        else if (menuID == "getState") {
            const bool needAck = obj.value("needAck").toBool(false);
            qDebug() << "[iScreenDF][TCP] getState requested needAck =" << needAck;

            if (localDFclient) {
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }
        else if (menuID == "setStreamEnable") {
            const bool enable = obj.value("enable").toBool(false);
            qDebug() << "[iScreenDF][TCP] setStreamEnable received: enable =" << enable;

            if (localDFclient) {
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }
        else if (menuID == "setDoaEnable") {
            const bool enable = obj.value("enable").toBool(false);
            qDebug() << "[iScreenDF][TCP] setDoaEnable received: enable =" << enable;

            p->m_setDoaEnable = enable;
            queueUpdateParameterField(db, "setDoaEnable", enable ? 1 : 0);

            if (localDFclient) {
                emit rfsocDoaFftUpdated(p->m_setDoaEnable, p->m_spectrumEnabled);
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }
        else if (menuID == "setSpectrumEnable") {
            const bool enable = obj.value("enable").toBool(false);
            qDebug() << "[iScreenDF][TCP] setSpectrumEnable received: enable =" << enable;

            p->m_spectrumEnabled = enable;
            queueUpdateParameterField(db, "spectrumEnabled", enable ? 1 : 0);

            if (localDFclient) {
                emit rfsocDoaFftUpdated(p->m_setDoaEnable, p->m_spectrumEnabled);
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }
        else if (menuID == "setAdcChannel") {
            const int ch = obj.value("channel").toInt(0);
            qDebug() << "[iScreenDF][TCP] setAdcChannel received: channel =" << ch;

            p->m_setAdcChannel = ch;
            queueUpdateParameterField(db, "setAdcChannel", ch);

            if (localDFclient) {
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }
        else if (menuID == "setFftConfig") {
            const int fftPoints     = obj.value("fft_points").toInt();
            const int fftDownsample = obj.value("fft_downsample").toInt();

            qDebug() << "[iScreenDF][TCP] setFftConfig received:"
                     << "fft_points =" << fftPoints
                     << "fft_downsample =" << fftDownsample;

            if (localDFclient) {
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }
        else if (menuID == "setTxHz") {
            const double hz = obj.value("hz").toDouble();
            qDebug() << "[iScreenDF][TCP] setTxHz received: hz =" << hz;

            p->m_txHz = hz;
            queueUpdateParameterField(db, "TxHz", p->m_txHz);

            if (localDFclient) {
                emit updateTxHzFromServer(p->m_txHz);
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }
        else if (menuID == "setDoaTargetOffsetHz") {
            const double offsetHz = obj.value("offset_hz").toDouble();
            qDebug() << "[iScreenDF][TCP] setDoaTargetOffsetHz received: offset_hz =" << offsetHz;

            p->m_TargetOffsetHz = offsetHz;
            queueUpdateParameterField(db, "TargetOffsetHz", p->m_TargetOffsetHz);

            if (localDFclient) {
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }
        else if (menuID == "setDoaBwHz") {
            const double bwHz = obj.value("bw_hz").toDouble();
            qDebug() << "[iScreenDF][TCP] setDoaBwHz received: bw_hz =" << bwHz;

            p->m_doaBwHz = bwHz;
            queueUpdateParameterField(db, "DoaBwHz", p->m_doaBwHz);

            if (localDFclient) {
                emit rfsocParameterUpdated(p->m_Frequency, p->m_doaBwHz);
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }
        else if (menuID == "setDoaPowerThresholdDb") {
            const double thDb = obj.value("th_db").toDouble(p->m_doaPowerThresholdDb);

            qDebug() << "[iScreenDF][TCP] setDoaPowerThresholdDb received:"
                     << "th_db =" << thDb
                     << "localDFclient =" << (localDFclient != nullptr);

            p->m_doaPowerThresholdDb = static_cast<float>(thDb);
            queueUpdateParameterField(db, "DoaPowerThresholdDb", thDb);

            if (localDFclient) {
                emit updateGateThDbFromServer(p->m_doaPowerThresholdDb);
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }
        else if (menuID == "setFrequencyHz") {
            const qint64 freqHz = obj.value("freq_hz").toVariant().toLongLong();
            const int updateEn = obj.value("update_en").toInt(-1);

            qDebug() << "[iScreenDF][TCP] setFrequencyHz received:"
                     << "freq_hz =" << freqHz
                     << "update_en =" << updateEn;

            p->m_Frequency = static_cast<int>(freqHz);
            p->m_update_en = updateEn;

            if (db) {
                QTimer::singleShot(0, db, [db = db, freqHz, updateEn]() {
                    db->UpdateParameterField("Frequency", freqHz);
                    db->UpdateParameterField("update_en", updateEn);
                });
            }

            if (localDFclient) {
                emit rfsocParameterUpdated(p->m_Frequency, p->m_doaBwHz);
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }
        else if (menuID == "setFcHz") {
            const qint64 fcHz = obj.value("fc_hz").toVariant().toLongLong();
            qDebug() << "[iScreenDF][TCP] setFcHz received: fc_hz =" << fcHz;

            if (localDFclient) {
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }
        else if (menuID == "setRfAgcEnable") {
            const int ch = obj.value("ch").toInt(-1);
            const bool enable = obj.value("enable").toBool(false);
            const bool needAck = obj.value("needAck").toBool(false);

            qDebug() << "[iScreenDF][TCP] setRfAgcEnable received:"
                     << "ch =" << ch
                     << "enable =" << enable
                     << "needAck =" << needAck;

            if (ch < 0) {
                p->m_rfAgcEnabled = enable;
                for (int i = 0; i < 5; ++i)
                    p->m_rfAgcChEnabled[i] = enable;

                queueUpdateParameterField(db, "rf_agc_enabled", enable ? 1 : 0);
            } else if (ch >= 0 && ch < 5) {
                p->m_rfAgcChEnabled[ch] = enable;
            } else {
                qWarning() << "[iScreenDF][TCP] setRfAgcEnable: invalid ch =" << ch;
                return;
            }

            emit updateRfAgcEnableFromServer(ch, enable);

            if (localDFclient) {
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }
        else if (menuID == "setRfAgcChannel") {
            const int ch = obj.value("ch").toInt(-1);
            const double targetDb = obj.value("target_db").toDouble(-70.0);
            const bool needAck = obj.value("needAck").toBool(false);

            qDebug() << "[iScreenDF][TCP] setRfAgcChannel received:"
                     << "ch =" << ch
                     << "target_db =" << targetDb
                     << "needAck =" << needAck;

            if (ch < 0) {
                for (int i = 0; i < 5; ++i)
                    p->m_rfAgcTargetDb[i] = targetDb;
            } else if (ch >= 0 && ch < 5) {
                p->m_rfAgcTargetDb[ch] = targetDb;
            } else {
                qWarning() << "[iScreenDF][TCP] setRfAgcChannel: invalid ch =" << ch;
            }

            queueUpdateParameterField(db, "rf_agc_target_db", targetDb);

            emit updateRfAgcTargetFromServer(ch, targetDb);

            if (localDFclient) {
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }
        else if (menuID == "setDoaAlgorithm") {
            const QString algo = obj.value("algo").toString().trimmed();
            const bool needAck = obj.value("needAck").toBool(false);

            qDebug() << "[iScreenDF][TCP] setDoaAlgorithm received:"
                     << "algo =" << algo
                     << "needAck =" << needAck;

            if (!algo.isEmpty()) {
                p->m_doaAlgorithm = algo;
                queueUpdateParameterField(db, "DoaAlgorithm", algo);
            }

            if (localDFclient) {
                emit updateDoaAlgorithmFromServer(p->m_doaAlgorithm);
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }
        else if (menuID == "setUcaRadiusM") {
            const double radiusM = obj.value("radius_m").toDouble();
            const bool needAck = obj.value("needAck").toBool(false);

            qDebug() << "[iScreenDF][TCP] setUcaRadiusM received:"
                     << "radius_m =" << radiusM
                     << "needAck =" << needAck;

            p->m_ucaRadiusM = radiusM;
            queueUpdateParameterField(db, "uca_radius_m", p->m_ucaRadiusM);

            if (localDFclient) {
                emit updateUcaRadiusFromServer(radiusM);
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }
        else if (menuID == "setScannerAttDb") {
            const double attDb = obj.value("att_db").toDouble(0.0);
            const bool needAck = obj.value("needAck").toBool(false);

            qDebug() << "[iScreenDF][TCP] setScannerAttDb received:"
                     << "att_db =" << attDb
                     << "needAck =" << needAck;

            p->m_scannerAttDb = attDb;

            if (localDFclient) {
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }
    } else {
        qDebug() << "[iScreenDF][TCP] Non-JSON message:" << message;

        if (localDFclient)
            localDFclient->sendLine(message.toUtf8(), true);
    }
}

void iScreenDF::onTcpClientConnected(const QHostAddress &addr,
                                     quint16 port)
{
    qDebug() << "[iScreenDF][TCP] Client connected from"
             << addr.toString() << ":" << port;

    QJsonObject hello;
    hello["menuID"] = "hello";
    hello["msg"]    = "Welcome to iScreenDF TCP server";

    QString jsonHello = QString::fromUtf8(
        QJsonDocument(hello).toJson(QJsonDocument::Compact));

    tcpServerDF->broadcastLine(jsonHello);
}

void iScreenDF::onTcpClientDisconnected(const QHostAddress &addr,
                                        quint16 port)
{
    qDebug() << "[iScreenDF][TCP] Client disconnected:"
             << addr.toString() << ":" << port;
}

void iScreenDF::updateReceiverParametersFreqandbw(int frequencyHz, int bandwidthHz)
{
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] updateReceiverParametersFreqandbw: m_parameter empty/null";
        return;
    }

    Parameter *p = m_parameter.first();

    p->m_Frequency = frequencyHz;
    p->m_doaBwHz = bandwidthHz;

    const double offsetHz = bandwidthHz * 1.0;
    updateReceiverParametersFreqOffsetBw(static_cast<qint64>(frequencyHz),
                                         offsetHz,
                                         static_cast<double>(bandwidthHz));

    QJsonObject obj;
    obj["menuID"]     = "updateReceiverFreqandbw";
    obj["Freq"]       = p->m_Frequency;
    obj["BW"]         = p->m_doaBwHz;
    obj["linkstatus"] = p->m_linkStatus;

    broadcastMessageServerandClient(obj);
    qDebug() << "[functionTcpServer] updateReceiverFreqandbw:" << obj;
}

static double clampDouble(double v, double lo, double hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

void iScreenDF::updateReceiverParametersFreqOffsetBw(qint64 rfHz,
                                                     double offsetHz,
                                                     double bwHz)
{
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] updateReceiverParametersFreqOffsetBw: no parameter";
        return;
    }

    Parameter *p = m_parameter.first();

    const double fsHz  = 240000.0;
    const double nyqHz = 0.49 * fsHz;

    offsetHz = clampDouble(offsetHz, -nyqHz, nyqHz);
    bwHz     = clampDouble(bwHz, 50.0, 0.45 * fsHz);

    const double rfHzD = static_cast<double>(rfHz);
    double fcHz = rfHzD - offsetHz;
    if (fcHz < 0.0)
        fcHz = 0.0;

    emit rfsocParameterUpdated(p->m_Frequency, p->m_doaBwHz);

    p->m_Frequency      = fcHz;
    p->m_TargetOffsetHz = offsetHz;
    p->m_doaBwHz        = bwHz;

    const qint64 frequencyValue    = static_cast<qint64>(p->m_Frequency);
    const double targetOffsetValue = p->m_TargetOffsetHz;
    const double doaBwValue        = p->m_doaBwHz;

    if (db) {
        QTimer::singleShot(0, db, [db = db,
                                   frequencyValue,
                                   targetOffsetValue,
                                   doaBwValue]() {
            db->UpdateParameterField("Frequency", frequencyValue);
            db->UpdateParameterField("TargetOffsetHz", targetOffsetValue);
            db->UpdateParameterField("DoaBwHz", doaBwValue);
        });
    }

    qDebug() << "[iScreenDF][updateReceiverParametersFreqOffsetBw]"
             << "rf_hz="     << rfHzD
             << "fc_hz="     << p->m_Frequency
             << "offset_hz=" << p->m_TargetOffsetHz
             << "bw_hz="     << p->m_doaBwHz
             << "update_en=" << p->m_update_en;

    {
        QJsonObject o;
        o["menuID"]    = "setFrequencyHz";
        o["freq_hz"]   = static_cast<double>(fcHz);
        o["update_en"] = p->m_update_en;
        o["needAck"]   = true;
        sendRfsocJsonLine(o, true);
    }

    {
        QJsonObject o;
        o["menuID"]    = "setDoaTargetOffsetHz";
        o["offset_hz"] = static_cast<double>(p->m_TargetOffsetHz);
        o["needAck"]   = true;
        sendRfsocJsonLine(o, true);
    }

    {
        QJsonObject o;
        o["menuID"]  = "setDoaBwHz";
        o["bw_hz"]   = static_cast<double>(p->m_doaBwHz);
        o["needAck"] = true;
        sendRfsocJsonLine(o, true);
    }
}

void iScreenDF::sendSetDoaEnable(bool enable)
{
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] sendSetDoaEnable: m_parameter empty/null";
        return;
    }

    Parameter *p = m_parameter.first();
    p->m_setDoaEnable = enable;

    queueUpdateParameterField(db, "setDoaEnable", p->m_setDoaEnable ? 1 : 0);

    QJsonObject obj;
    obj["menuID"]  = "setDoaEnable";
    obj["enable"]  = enable;
    obj["needAck"] = true;

    sendRfsocJsonLine(obj, true);
}

void iScreenDF::sendSetSpectrumEnable(bool enable)
{
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] sendSetSpectrumEnable: m_parameter empty/null";
        return;
    }

    Parameter *p = m_parameter.first();
    p->m_spectrumEnabled = enable;

    queueUpdateParameterField(db, "spectrumEnabled", p->m_spectrumEnabled ? 1 : 0);

    emit rfsocDoaFftUpdated(p->m_setDoaEnable, p->m_spectrumEnabled);

    QJsonObject obj;
    obj["menuID"]  = "setSpectrumEnable";
    obj["enable"]  = enable;
    obj["needAck"] = true;

    sendRfsocJsonLine(obj, true);
}

void iScreenDF::sendGateThDb(double v)
{
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] sendGateThDb: m_parameter empty/null";
        return;
    }

    if (v < -140.0)
        v = -140.0;
    if (v > 0.0)
        v = 0.0;

    Parameter *p = m_parameter.first();
    qDebug() << "[iScreenDF] sendGateThDb from QML =" << v;

    p->m_doaPowerThresholdDb = static_cast<float>(v);
    m_blockUiSync = true;

    queueUpdateParameterField(db, "DoaPowerThresholdDb", p->m_doaPowerThresholdDb);

    if (localDFclient) {
        QJsonObject obj;
        obj["menuID"]  = "setDoaPowerThresholdDb";
        obj["th_db"]   = p->m_doaPowerThresholdDb;
        obj["needAck"] = true;

        localDFclient->sendLine(QJsonDocument(obj).toJson(QJsonDocument::Compact), true);
    }

    m_blockUiSync = false;
}

void iScreenDF::sendTxHz(double v)
{
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] sendTxHz: m_parameter empty/null";
        return;
    }

    if (v < 0.2)
        v = 0.2;
    if (v > 60.0)
        v = 60.0;

    qDebug() << "[iScreenDF] sendTxHz from QML =" << v;

    Parameter *p = m_parameter.first();
    m_blockUiSync = true;

    p->m_txHz = v;
    queueUpdateParameterField(db, "TxHz", p->m_txHz);

    if (localDFclient) {
        QJsonObject obj;
        obj["menuID"]  = "setTxHz";
        obj["hz"]      = p->m_txHz;
        obj["needAck"] = true;
        localDFclient->sendLine(QJsonDocument(obj).toJson(QJsonDocument::Compact), true);
    }

    QTimer::singleShot(0, this, [this]() {
        m_blockUiSync = false;
    });
}

void iScreenDF::sendDoaAlgorithm(const QString &algo)
{
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] sendDoaAlgorithm: m_parameter empty/null";
        return;
    }

    Parameter *p = m_parameter.first();
    qDebug() << "[iScreenDF] sendDoaAlgorithm from QML =" << algo;

    m_blockUiSync = true;
    p->m_doaAlgorithm = algo;

    queueUpdateParameterField(db, "DoaAlgorithm", algo);

    emit updateDoaAlgorithmFromServer(algo);

    if (localDFclient) {
        QJsonObject obj;
        obj["menuID"]  = "setDoaAlgorithm";
        obj["algo"]    = algo;
        obj["needAck"] = true;
        localDFclient->sendLine(QJsonDocument(obj).toJson(QJsonDocument::Compact), true);
    }

    QTimer::singleShot(0, this, [this]() {
        m_blockUiSync = false;
    });
}

void iScreenDF::sendUcaRadiusM(double radiusM)
{
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] sendUcaRadiusM: m_parameter empty/null";
        return;
    }

    Parameter *p = m_parameter.first();

    if (radiusM < 0.01)
        radiusM = 0.01;
    if (radiusM > 10.0)
        radiusM = 10.0;

    qDebug() << "[iScreenDF] sendUcaRadiusM from QML =" << radiusM;

    m_blockUiSync = true;
    p->m_ucaRadiusM = radiusM;

    queueUpdateParameterField(db, "uca_radius_m", p->m_ucaRadiusM);

    emit updateUcaRadiusFromServer(radiusM);

    if (localDFclient) {
        QJsonObject obj;
        obj["menuID"]   = "setUcaRadiusM";
        obj["radius_m"] = radiusM;
        obj["needAck"]  = true;

        localDFclient->sendLine(QJsonDocument(obj).toJson(QJsonDocument::Compact), true);
    }

    QTimer::singleShot(0, this, [this]() {
        m_blockUiSync = false;
    });
}

static double clampRfAgcDb(double v)
{
    if (std::isnan(v) || std::isinf(v))
        v = -60.0;
    if (v < -90.0)
        v = -90.0;
    if (v > -30.0)
        v = -30.0;
    return v;
}

void iScreenDF::sendRfAgcTargetAllDb(double targetDb)
{
    targetDb = clampRfAgcDb(targetDb);

    qDebug() << "[iScreenDF] sendRfAgcTargetAllDb from QML db=" << targetDb;

    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] sendRfAgcTargetAllDb: m_parameter empty/null";
        return;
    }

    Parameter *p = m_parameter.first();
    m_blockUiSync = true;

    for (int i = 0; i < 5; ++i)
        p->m_rfAgcTargetDb[i] = targetDb;

    queueUpdateParameterField(db, "rf_agc_target_db", targetDb);

    if (localDFclient) {
        for (int ch = 0; ch < 5; ++ch) {
            QJsonObject obj;
            obj["menuID"]    = "setRfAgcChannel";
            obj["ch"]        = ch;
            obj["target_db"] = targetDb;
            obj["needAck"]   = true;

            qDebug() << "[iScreenDF] sendRfAgcTargetAllDb: setRfAgcChannel ch =" << ch;
            localDFclient->sendLine(QJsonDocument(obj).toJson(QJsonDocument::Compact), true);
        }
    } else {
        qWarning() << "[iScreenDF] sendRfAgcTargetAllDb: localDFclient is null";
    }

    for (int ch = 0; ch < 5; ++ch)
        emit updateRfAgcTargetFromServer(ch, targetDb);

    QTimer::singleShot(0, this, [this]() {
        m_blockUiSync = false;
    });
}

void iScreenDF::sendRfAgcTargetDb(int ch, double targetDb)
{
    targetDb = clampRfAgcDb(targetDb);

    qDebug() << "[iScreenDF] sendRfAgcTargetDb from QML ch=" << ch << "db=" << targetDb;

    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] sendRfAgcTargetDb: m_parameter empty/null";
        return;
    }

    Parameter *p = m_parameter.first();
    m_blockUiSync = true;

    if (ch < 0) {
        for (int i = 0; i < 5; ++i)
            p->m_rfAgcTargetDb[i] = targetDb;
    } else if (ch >= 0 && ch < 5) {
        p->m_rfAgcTargetDb[ch] = targetDb;
    } else {
        qWarning() << "[iScreenDF] sendRfAgcTargetDb: invalid ch =" << ch;
        m_blockUiSync = false;
        return;
    }

    if (localDFclient) {
        QJsonObject obj;
        obj["menuID"]    = "setRfAgcChannel";
        obj["ch"]        = ch;
        obj["target_db"] = targetDb;
        obj["needAck"]   = true;

        qDebug() << "[iScreenDF] sendRfAgcTargetDb: setRfAgcChannel ch =" << ch;
        localDFclient->sendLine(QJsonDocument(obj).toJson(QJsonDocument::Compact), true);
    } else {
        qWarning() << "[iScreenDF] sendRfAgcTargetDb: localDFclient is null";
    }

    emit updateRfAgcTargetFromServer(ch, targetDb);

    QTimer::singleShot(0, this, [this]() {
        m_blockUiSync = false;
    });
}

void iScreenDF::sendRfAgcEnable(int ch, bool enable)
{
    qDebug() << "[iScreenDF] sendRfAgcEnable from QML ch=" << ch << "enable=" << enable;

    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] sendRfAgcEnable: m_parameter empty/null";
        return;
    }

    Parameter *p = m_parameter.first();
    m_blockUiSync = true;

    if (ch < 0) {
        p->m_rfAgcEnabled = enable;
        for (int i = 0; i < 5; ++i)
            p->m_rfAgcChEnabled[i] = enable;
    } else if (ch >= 0 && ch < 5) {
        p->m_rfAgcChEnabled[ch] = enable;
    } else {
        qWarning() << "[iScreenDF] sendRfAgcEnable: invalid ch =" << ch;
        m_blockUiSync = false;
        return;
    }

    queueUpdateParameterField(db, "rf_agc_enabled", enable ? 1 : 0);

    if (localDFclient) {
        QJsonObject obj;
        obj["menuID"]  = "setRfAgcEnable";
        obj["ch"]      = ch;
        obj["enable"]  = enable;
        obj["needAck"] = true;

        localDFclient->sendLine(QJsonDocument(obj).toJson(QJsonDocument::Compact), true);
    } else {
        qWarning() << "[iScreenDF] sendRfAgcEnable: localDFclient is null";
    }

    emit updateRfAgcEnableFromServer(ch, enable);

    QTimer::singleShot(0, this, [this]() {
        m_blockUiSync = false;
    });
}

void iScreenDF::setLinkStatus(bool linkStatus)
{
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] setLinkStatus: m_parameter empty/null";
        return;
    }

    Parameter *p = m_parameter.first();
    p->m_linkStatus = linkStatus;

    qDebug() << "[iScreenDF] setLinkStatusFromQml =" << linkStatus;

    queueUpdateParameterField(db, "linkstatus", linkStatus ? 1 : 0);
}

QString iScreenDF::sanitizePathPart(QString s)
{
    s = s.trimmed();
    s.replace(QRegularExpression(R"(\s+)"), "_");
    s.replace(QRegularExpression(R"([^A-Za-z0-9_\-\.])"), "_");

    if (s.isEmpty())
        s = "NA";

    return s;
}

QString iScreenDF::freqToFolder(double freqHz)
{
    if (!qIsFinite(freqHz) || freqHz <= 0)
        return "0MHz";

    const double mhz = freqHz / 1e6;
    QString s = QString::number(mhz, 'f', 3);
    s.replace('.', '_');

    return s + "MHz";
}

QString iScreenDF::dateToFolder(const QString &dateStr, double ms)
{
    QDate d = QDate::fromString(dateStr.trimmed(), "dd MMM yyyy");

    if (!d.isValid()) {
        const qint64 msi = static_cast<qint64>(ms);
        if (msi > 0)
            d = QDateTime::fromMSecsSinceEpoch(msi).date();
    }

    if (!d.isValid())
        d = QDate::currentDate();

    return d.toString("yyyy-MM-dd");
}

QString iScreenDF::timeToFolder(const QString &timeStr, double ms)
{
    QTime t = QTime::fromString(timeStr.trimmed(), "HH:mm:ss");

    if (!t.isValid()) {
        const qint64 msi = static_cast<qint64>(ms);
        if (msi > 0)
            t = QDateTime::fromMSecsSinceEpoch(msi).time();
    }

    if (!t.isValid())
        t = QTime::currentTime();

    return t.toString("HH-mm-ss");
}

QString iScreenDF::buildDailyCsvPath(double freqHz,
                                     const QString &dateStr,
                                     double updatedMs) const
{
    const QString freqFolder = sanitizePathPart(freqToFolder(freqHz));
    const QString dayFolder  = sanitizePathPart(dateToFolder(dateStr, updatedMs));

    const QString dirPath  = m_txBaseDir + "/" + freqFolder + "/" + dayFolder;
    const QString filePath = dirPath + "/" + freqFolder + "_" + dayFolder + ".csv";

    return filePath;
}

bool iScreenDF::ensureDir(const QString &dirPath)
{
    QDir dir;
    return dir.mkpath(dirPath);
}

void iScreenDF::updateActiveCsvIfNeeded(double freqHz,
                                        const QString &dateStr,
                                        double updatedMs)
{
    const QString newFreqFolder = sanitizePathPart(freqToFolder(freqHz));
    const QString newDayFolder  = sanitizePathPart(dateToFolder(dateStr, updatedMs));
    const QString newPath       = buildDailyCsvPath(freqHz, dateStr, updatedMs);

    if (newFreqFolder == m_activeFreqFolder &&
        newDayFolder  == m_activeDayFolder  &&
        newPath       == m_activeCsvPath)
    {
        return;
    }

    m_activeFreqFolder = newFreqFolder;
    m_activeDayFolder  = newDayFolder;
    m_activeCsvPath    = newPath;

    m_lastTxSeenKey.clear();
    m_lastTxWrittenKey.clear();

    const QFileInfo fi(m_activeCsvPath);
    ensureDir(fi.absolutePath());
}

bool iScreenDF::appendTxCsvRow(const QString &filePath,
                               const QString &latStr,
                               const QString &lonStr,
                               const QString &rmsStr,
                               const QString &freqStr,
                               const QString &dateStr,
                               const QString &timeStr,
                               const QString &updatedMsStr,
                               const QString &mgrs)
{
    Q_UNUSED(latStr)
    Q_UNUSED(lonStr)

    const QFileInfo fi(filePath);
    if (!ensureDir(fi.absolutePath())) {
        qWarning().noquote() << "[TX CSV] mkpath failed for" << fi.absolutePath();
        return false;
    }

    const bool newFile = !QFile::exists(filePath);

    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning().noquote() << "[TX CSV] open failed:" << filePath << "err=" << f.errorString();
        return false;
    }

    QTextStream out(&f);
    out.setCodec("UTF-8");

    if (newFile)
        out << "rms_m,freqHz,date,time,updatedMs,mgrs\n";

    out << rmsStr << ","
        << freqStr << ","
        << dateStr << ","
        << timeStr << ","
        << updatedMsStr << ","
        << mgrs
        << "\n";

    out.flush();
    f.close();

    return true;
}

void iScreenDF::onTxSnapshotUpdated(double lat,
                                    double lon,
                                    double rms_m,
                                    double freqHz,
                                    const QString &dateStr,
                                    const QString &timeStr,
                                    double updatedMs,
                                    const QString &mgrs)
{
    const QString latStr = QString::number(lat, 'f', 6);
    const QString lonStr = QString::number(lon, 'f', 6);
    const QString rmsStr = QString::number(rms_m,  'f', 2);
    const QString freqStr = QString::number(freqHz, 'f', 0);
    const QString upMsStr = QString::number(updatedMs, 'f', 0);

    const QString key =
        latStr + "," + lonStr +
        "|rms=" + rmsStr +
        "|f=" + freqStr +
        "|mgrs=" + mgrs;

    updateActiveCsvIfNeeded(freqHz, dateStr, updatedMs);

    if (key != m_lastTxSeenKey) {
        m_lastTxSeenKey = key;
        return;
    }

    if (key == m_lastTxWrittenKey)
        return;

    m_lastTxWrittenKey = key;

    const QString csvPath = m_activeCsvPath;

    const bool ok = appendTxCsvRow(csvPath,
                                   latStr,
                                   lonStr,
                                   rmsStr,
                                   freqStr,
                                   dateStr,
                                   timeStr,
                                   upMsStr,
                                   mgrs);

    if (ok) {
        qDebug().noquote()
        << "[TX CSV APPEND]"
        << csvPath
        << "lat=" << latStr
        << "lon=" << lonStr
        << "rms_m=" << rmsStr
        << "freqHz=" << freqStr
        << "date=" << dateStr
        << "time=" << timeStr
        << "updatedMs=" << upMsStr
        << "mgrs=" << mgrs;
    } else {
        qWarning().noquote() << "[TX CSV] write failed:" << csvPath;
    }
}

void iScreenDF::requestSaveDoaLogSelectedJson(const QString &jsonText)
{
    qDebug() << "[DOA SAVE SLOT] jsonText =" << jsonText;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &err);

    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[DOA SAVE SLOT] invalid JSON:" << err.errorString();
        return;
    }

    QJsonObject root = doc.object();

    if (root.value("objectName").toString() != "DoaLogSaveSelectedRows") {
        qWarning() << "[DOA SAVE SLOT] invalid objectName:"
                   << root.value("objectName").toString();
        return;
    }

    QJsonArray records = root.value("records").toArray();

    if (records.isEmpty()) {
        qWarning() << "[DOA SAVE SLOT] records is empty";
        return;
    }

    qDebug() << "[DOA SAVE SLOT] count =" << records.size();

    for (const QJsonValue &v : records) {
        if (!v.isObject())
            continue;

        QJsonObject r = v.toObject();

        QString timestamp  = r.value("timestamp").toString();
        QString name       = r.value("name").toString();
        QString frequency  = r.value("frequency").toString();
        QString doa        = r.value("doa").toString();
        QString extra      = r.value("extra").toString();
        QString key        = r.value("key").toString();
        QString confidence = r.value("confidence").toString();
        QString heading    = r.value("heading").toString();
        QString lat        = r.value("lat").toString();
        QString lon        = r.value("lon").toString();

        qDebug() << "[DOA RECORD]"
                 << "timestamp=" << timestamp
                 << "name=" << name
                 << "frequency=" << frequency
                 << "doa=" << doa
                 << "extra=" << extra
                 << "key=" << key
                 << "confidence=" << confidence
                 << "heading=" << heading
                 << "lat=" << lat
                 << "lon=" << lon;
    }

    if (!db) {
        qWarning() << "[DOA SAVE SLOT] db is null";
        return;
    }

    const int recordCount = records.size();

    QTimer::singleShot(0, db, [db = db, jsonText, recordCount]() {
        const bool ok = db->insertDoaLogRecordsFromJson(jsonText);

        if (!ok) {
            qWarning() << "[DOA SAVE SLOT] insert DOA logs failed";
            return;
        }

        qDebug() << "[DOA SAVE SLOT] insert DOA logs success. count =" << recordCount;
    });
}

void iScreenDF::requestDeleteDoaLogsJson(const QString &jsonText)
{
    qDebug() << "[KrakenMapVal] requestDeleteDoaLogsJson:" << jsonText;

    if (!db) {
        qWarning() << "[KrakenMapVal] requestDeleteDoaLogsJson: db is null";
        return;
    }

    QTimer::singleShot(0, db, [db = db, jsonText]() {
        db->deleteDoaLogsFromJson(jsonText);
    });
}
