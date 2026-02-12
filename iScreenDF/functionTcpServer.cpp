#include "iScreenDF.h"

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
    if (parts.size() != 4) return -1;

    int prefix = 0;
    bool zeroSeen = false;

    for (const QString &p : parts) {
        bool ok = false;
        int oct = p.toInt(&ok);
        if (!ok || oct < 0 || oct > 255) return -1;

        for (int b = 7; b >= 0; --b) {
            const bool bit = (oct >> b) & 1;
            if (bit) {
                if (zeroSeen) return -1; // 1 หลัง 0 = mask ผิดรูป
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
    if (pfx <= 0) return ip;             // fallback
    return ip + "/" + QString::number(pfx);
}

static QString makeDnsString(const QString &dns1, const QString &dns2)
{
    QStringList list;
    if (!dns1.trimmed().isEmpty()) list << dns1.trimmed();
    if (!dns2.trimmed().isEmpty()) list << dns2.trimmed();
    return list.join(",");
}

static QString makeModeFromDhcp(const QString &dhcp)
{
    const QString v = dhcp.trimmed().toLower();
    // รองรับค่าได้หลายแบบ
    if (v == "1" || v == "true" || v == "dhcp" || v == "on" || v == "enable")
        return "dhcp";
    return "static";
}
void iScreenDF::requestRfFrequency(){
    if (m_parameter.isEmpty() || !m_parameter.first()) return;

    Parameter *p = m_parameter.first();
    emit rfsocParameterUpdated(p->m_Frequency, p->m_doaBwHz);

    emit updateDoaLineMeters( p->m_maxDoaLine_meters);
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

    // ใช้ IP แบบปกติ (ไม่ใช่ CIDR)
    obj["ip"]      = ip;
    obj["netmask"] = mask;
    obj["gateway"] = gw;

    obj["dns1"]    = dns1;
    obj["dns2"]    = dns2;

    // obj["needAck"] = true;

    // ============================
    // DEBUG JSON (แนะนำ)
    // ============================
    QJsonDocument doc(obj);
    qDebug().noquote()
        << "[iScreenDF][setIpConfig][JSON] ="
        << doc.toJson(QJsonDocument::Compact);

    sendRfsocJsonLine(obj, true);
}

void iScreenDF::GetrfsocParameter(bool  setDoaEnable, bool spectrumEnabled, int setAdcChannel,
                                  int Frequency, int update_en, double TxHz,
                                  int TargetOffsetHz, int DoaBwHz, double DoaPowerThresholdDb,
                                  const QString &DoaAlgorithm, double ucaRadiusM,
                                  double TargetDb, bool rfAgcEnabled,bool linkStatus
                                  ,double offsetvalue, double compassoffset, int maxDoaLineMeters,const QString &ipLocalForRemoteGroup, int setDelayMs,int setDistance)
{
    qDeleteAll(m_parameter);
    m_parameter.clear();

    Parameter *p = new Parameter();
    p->m_setDoaEnable        = setDoaEnable;
    p->m_spectrumEnabled     = spectrumEnabled;
    p->m_setAdcChannel       = setAdcChannel;
    p->m_Frequency           = Frequency;
    p->m_update_en           = update_en;
    p->m_txHz                = TxHz;
    p->m_TargetOffsetHz      = TargetOffsetHz;
    p->m_doaBwHz             = DoaBwHz;
    p->m_doaPowerThresholdDb = static_cast<float>(DoaPowerThresholdDb);
    p->m_doaAlgorithm        = DoaAlgorithm;
    p->m_ucaRadiusM          = ucaRadiusM;
    p->m_offset_value        = offsetvalue;
    p->m_compass_offset      = compassoffset;
    p->m_maxDoaLine_meters  = maxDoaLineMeters;
    p->m_ipLocalForRemoteGroup = ipLocalForRemoteGroup;
    // =========================
    // RF AGC: ใช้ค่าจาก DB จริง
    // =========================
    p->m_rfAgcEnabled = rfAgcEnabled;
    p->m_linkStatus = linkStatus;

    for (int i = 0; i < 5; ++i)
        p->m_rfAgcChEnabled[i] = p->m_rfAgcEnabled;

    auto clampRfAgcDbLocal = [](double v) -> double {
        if (v < -90.0) v = -90.0;
        if (v > -30.0) v = -30.0;
        return v;
    };
    TargetDb = clampRfAgcDbLocal(TargetDb);

    for (int i = 0; i < 5; ++i)
        p->m_rfAgcTargetDb[i] = TargetDb;

    // ===== debug init =====
    qDebug() << "[iScreenDF] init rf_agc_enabled =" << p->m_rfAgcEnabled
             << "rf_agc_target_db(all)=" << TargetDb;

    // ===== push to QML =====
    // 1) checkbox ตัวเดียว (all)
    emit updateRfAgcEnableFromServer(-1, p->m_rfAgcEnabled);

    // 2) target_db ส่งทีละ ch=0..4 (เพราะ QML รวมเป็น slider เดียวจาก 0..4)
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
    emit updateGlobalOffsets( p->m_offset_value, p->m_compass_offset);
    emit updateDoaLineMeters( p->m_maxDoaLine_meters);
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
    if (m_parameter.isEmpty() || !m_parameter.first()) return;

    Parameter *p = m_parameter.first();
    qDebug() << "[iScreenDF] sendMaxDoaLineMeters =" << ip ;
    p->m_ipLocalForRemoteGroup  = ip;
    db->UpdateParameterField("IPLocalForRemoteGroup", ip);
    emit updateIPLocalForRemoteGroupFromServer( p->m_ipLocalForRemoteGroup);
}
void iScreenDF::sendMaxDoaLineMeters(int meters)
{
    if (m_parameter.isEmpty() || !m_parameter.first()) return;

    Parameter *p = m_parameter.first();
    qDebug() << "[iScreenDF] sendMaxDoaLineMeters =" << meters ;
    p->m_maxDoaLine_meters  = meters;
    db->UpdateParameterField("maxDoaLineMeters", meters);
    emit updateDoaLineMeters( p->m_maxDoaLine_meters);
}

void iScreenDF::setCompassOffset(double offset)
{
    if (m_parameter.isEmpty() || !m_parameter.first()) return;

    Parameter *p = m_parameter.first();
    p->m_compass_offset = offset;

    qDebug() << "[Compass] setCompassOffset =" << offset;
    db->UpdateParameterField("compass_offset",p->m_compass_offset);
    emit updateGlobalOffsets(p->m_offset_value, p->m_compass_offset);
}

// void iScreenDF::GetIPDFServer(const QString &ip)
// {
//     if (m_parameter.isEmpty() || !m_parameter.first()) {
//         qWarning() << "[iScreenDF] applyRfsocParameterToServer: no parameter";
//         return;
//     }
//     Parameter *p = m_parameter.first();
//     p->m_ipdfServer = ip;
//     localDFclient->connectToServer(p->m_ipdfServer,5555);
//     emit updateServeripDfserver(p->m_ipdfServer);
// }

void iScreenDF::GetIPDFServer(const QString &ip)
{
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] applyRfsocParameterToServer: no parameter";
        return;
    }

    Parameter *p = m_parameter.first();
    p->m_ipdfServer = ip;

    // ✅ DF server connect
    localDFclient->connectToServer(p->m_ipdfServer, 5555);
    emit updateServeripDfserver(p->m_ipdfServer);

    // ✅ GPSD connect ด้วย (ip เดียวกัน)
    if (gpsReader) {
        gpsReader->setGpsdEndpoint(p->m_ipdfServer, 2947);
        gpsReader->start(); // ถ้ารันอยู่แล้วจะไม่สร้าง worker ใหม่ แต่จะ reconnectRequested_
    }
}


void iScreenDF::updateIPServerDF()
{
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] applyRfsocParameterToServer: no parameter";
        return;
    }
    Parameter *p = m_parameter.first();
    qDebug() <<  "[iScreenDF] ServeripDfserver" << p->m_ipdfServer;
    emit updateServeripDfserver(p->m_ipdfServer);
}

void iScreenDF::connectToDFserver(const QString &ip)
{
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] applyRfsocParameterToServer: no parameter ip:" << ip;
        return;
    }
    Parameter *p = m_parameter.first();
    p->m_ipdfServer = ip;
    db->UpdateParameterField("ipdfserver", p->m_ipdfServer);
    qDebug() <<  "[iScreenDF] connectToDFserver" << p->m_ipdfServer;
    localDFclient->connectToServer(p->m_ipdfServer,5555);

    if (gpsReader) {
        gpsReader->setGpsdEndpoint(p->m_ipdfServer, 2947);
        gpsReader->start();
    }

}

void iScreenDF::applyRfsocParameterToServer(bool needAck /*=true*/)
{
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] applyRfsocParameterToServer: no parameter";
        return;
    }
    const Parameter *p = m_parameter.first();

    // 1) DoA enable
    {
        QJsonObject o;
        o["menuID"]  = "setDoaEnable";
        o["enable"]  = p->m_setDoaEnable;
        o["needAck"] = needAck;
        sendRfsocJsonLine(o, true);
    }

    // 2) Spectrum enable
    {
        QJsonObject o;
        o["menuID"]  = "setSpectrumEnable";
        o["enable"]  = p->m_spectrumEnabled;
        o["needAck"] = needAck;
        sendRfsocJsonLine(o, true);
    }

    // 3) ADC channel
    {
        QJsonObject o;
        o["menuID"]  = "setAdcChannel";
        o["channel"] = p->m_setAdcChannel;
        o["needAck"] = needAck;
        sendRfsocJsonLine(o, true);
    }

    // 4) Frequency + update_en (NCO update mask)
    {
        QJsonObject o;
        o["menuID"]    = "setFrequencyHz";
        o["freq_hz"]   = p->m_Frequency;
        o["update_en"] = p->m_update_en;
        o["needAck"]   = needAck;
        sendRfsocJsonLine(o, true);
    }

    // 5) TxHz (อย่า cast เป็น int ตอนส่ง ถ้าคุณต้องการทศนิยม)
    {
        QJsonObject o;
        o["menuID"]  = "setTxHz";
        o["hz"]      = p->m_txHz; // ถ้าจะเก็บเป็น int ก็ส่งเป็น int ได้
        o["needAck"] = needAck;
        sendRfsocJsonLine(o, true);
    }

    // 6) TargetOffsetHz
    {
        QJsonObject o;
        o["menuID"]    = "setDoaTargetOffsetHz";
        o["offset_hz"] = p->m_TargetOffsetHz;
        o["needAck"]   = needAck;
        sendRfsocJsonLine(o, true);
    }

    // 7) DoA BW
    {
        QJsonObject o;
        o["menuID"]  = "setDoaBwHz";
        o["bw_hz"]   = p->m_doaBwHz;
        o["needAck"] = needAck;
        sendRfsocJsonLine(o, true);
    }

    // 8) Threshold dB
    {
        QJsonObject o;
        o["menuID"]  = "setDoaPowerThresholdDb";
        o["th_db"]   = static_cast<double>(p->m_doaPowerThresholdDb);
        o["needAck"] = needAck;
        sendRfsocJsonLine(o, true);
    }
    // 9)setUcaRadiusM
    {
        QJsonObject o;
        o["menuID"]  = "setUcaRadiusM";
        o["radius_m"]   = p->m_ucaRadiusM;
        o["needAck"] = needAck;
        sendRfsocJsonLine(o, true);
    }
    // 10)setDoaAlgorithm
    {
        QJsonObject o;
        o["menuID"]  = "setDoaAlgorithm";
        o["algo"]   = p->m_doaAlgorithm;
        o["needAck"] = needAck;
        sendRfsocJsonLine(o, true);
    }

    {
        QJsonObject o;
        o["menuID"]  = "setRfAgcEnable";
        o["enable"] = p->m_rfAgcEnabled;
        o["ch"] = -1;
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

    // if (m_parameter.isEmpty() || !m_parameter.first()) {
    //     qWarning() << "[iScreenDF] onTcpMessage: no parameter";
    //     return;
    // }

    // Parameter *p = m_parameter.first();
    // if (!p) {
    //     qWarning() << "[iScreenDF] onTcpMessage: no parameter";
    //     return;
    // }

    // // ✅ ใช้ IP จาก Parameter แทน m_network2List.at(1)->ip_address
    // const QString localIp = p->m_ipLocalForRemoteGroup.trimmed();
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] applyRfsocParameterToServer: no parameter";
        return;
    }
    Parameter *p = m_parameter.first();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {

        QJsonObject obj = doc.object();
        const QString menuID = obj.value("menuID").toString();

        // ============ 1) client ขอเช็ค status ============
        if (menuID == "getStatus") {
            QJsonObject reply;
            reply["menuID"] = "statusReply";
            reply["msg"]    = "OK from iScreenDF";
            reply["ip"] = p->m_ipLocalForRemoteGroup;

            const QString jsonReply = QString::fromUtf8(
                QJsonDocument(reply).toJson(QJsonDocument::Compact));

            tcpServerDF->broadcastLine(jsonReply);

            // ✅ forward (ถ้าต้องการ forward reply ด้วย)
            // if (localDFclient) {
            //     localDFclient->sendLine(jsonReply.toUtf8(), true);
            // }
        }

        // ============ 2) client ขอชื่อเครื่อง / serial ============
        else if (menuID == "getName") {
            QJsonObject reply;
            reply["menuID"] = "getName";
            reply["name"]   = controllerName;
            reply["serial"] = Serialnumber;
            reply["ip"] = p->m_ipLocalForRemoteGroup;

            const QString jsonReply = QString::fromUtf8(
                QJsonDocument(reply).toJson(QJsonDocument::Compact));

            tcpServerDF->broadcastLine(jsonReply);

            // ✅ forward (ถ้าต้องการ forward reply ด้วย)
            // if (localDFclient) {
            //     localDFclient->sendLine(jsonReply.toUtf8(), true);
            // }
        }

        // ============ 3) getState ============
        else if (menuID == "getState") {
            bool needAck = obj.value("needAck").toBool(false);
            qDebug() << "[iScreenDF][TCP] getState requested"
                     << "needAck =" << needAck;

            if (localDFclient) {
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }

        // ============ 4) setStreamEnable ============
        else if (menuID == "setStreamEnable") {
            bool enable = obj.value("enable").toBool(false);
            qDebug() << "[iScreenDF][TCP] setStreamEnable received:"
                     << "enable =" << enable;

            if (localDFclient) {
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }

        // ============ 5) setDoaEnable ============
        else if (menuID == "setDoaEnable") {
            bool enable = obj.value("enable").toBool(false);
            qDebug() << "[iScreenDF][TCP] setDoaEnable received:"
                     << "enable =" << enable;

            p->m_setDoaEnable = enable;
            if (db) db->UpdateParameterField("setDoaEnable", enable ? 1 : 0);

            if (localDFclient) {
                emit rfsocDoaFftUpdated(p->m_setDoaEnable, p->m_spectrumEnabled);
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }

        // ============ 6) setSpectrumEnable ============
        else if (menuID == "setSpectrumEnable") {
            bool enable = obj.value("enable").toBool(false);
            qDebug() << "[iScreenDF][TCP] setSpectrumEnable received:"
                     << "enable =" << enable;

            p->m_spectrumEnabled = enable;
            if (db) db->UpdateParameterField("spectrumEnabled", enable ? 1 : 0);

            if (localDFclient) {
                emit rfsocDoaFftUpdated(p->m_setDoaEnable, p->m_spectrumEnabled);
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }

        // ============ 7) setAdcChannel ============
        else if (menuID == "setAdcChannel") {
            int ch = obj.value("channel").toInt(0);
            qDebug() << "[iScreenDF][TCP] setAdcChannel received:"
                     << "channel =" << ch;

            p->m_setAdcChannel = ch;
            if (db) db->UpdateParameterField("setAdcChannel", ch);

            if (localDFclient) {
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }

        // ============ 8) setFftConfig ============
        else if (menuID == "setFftConfig") {
            int fftPoints     = obj.value("fft_points").toInt();
            int fftDownsample = obj.value("fft_downsample").toInt();
            qDebug() << "[iScreenDF][TCP] setFftConfig received:"
                     << "fft_points =" << fftPoints
                     << "fft_downsample =" << fftDownsample;

            if (localDFclient) {
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }

        // ============ 9) setTxHz ============
        else if (menuID == "setTxHz") {
            double hz = obj.value("hz").toDouble();
            qDebug() << "[iScreenDF][TCP] setTxHz received:"
                     << "hz =" << hz;

            p->m_txHz = hz;
            if (db) db->UpdateParameterField("TxHz", p->m_txHz);

            if (localDFclient) {
                emit updateTxHzFromServer(p->m_txHz);
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }

        // ============ 10) setDoaTargetOffsetHz ============
        else if (menuID == "setDoaTargetOffsetHz") {
            double offsetHz = obj.value("offset_hz").toDouble();
            qDebug() << "[iScreenDF][TCP] setDoaTargetOffsetHz received:"
                     << "offset_hz =" << offsetHz;

            p->m_TargetOffsetHz = offsetHz;
            if (db) db->UpdateParameterField("TargetOffsetHz", offsetHz);

            if (localDFclient) {
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }

        // ============ 11) setDoaBwHz ============
        else if (menuID == "setDoaBwHz") {
            double bwHz = obj.value("bw_hz").toDouble();
            qDebug() << "[iScreenDF][TCP] setDoaBwHz received:"
                     << "bw_hz =" << bwHz;

            p->m_doaBwHz = bwHz;
            if (db) db->UpdateParameterField("DoaBwHz", bwHz);

            if (localDFclient) {
                emit rfsocParameterUpdated(p->m_Frequency, p->m_doaBwHz);
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }

        // ============ 12) setDoaPowerThresholdDb ============
        else if (menuID == "setDoaPowerThresholdDb") {
            const double thDb = obj.value("th_db").toDouble(p->m_doaPowerThresholdDb);

            qDebug() << "[iScreenDF][TCP] setDoaPowerThresholdDb received:"
                     << "th_db =" << thDb
                     << "localDFclient =" << (localDFclient != nullptr);

            p->m_doaPowerThresholdDb = static_cast<float>(thDb);

            if (db) db->UpdateParameterField("DoaPowerThresholdDb", thDb);

            if (localDFclient) {
                emit updateGateThDbFromServer(p->m_doaPowerThresholdDb);
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }

        // ============ 13) setFrequencyHz ============
        else if (menuID == "setFrequencyHz") {
            qint64 freqHz = obj.value("freq_hz").toVariant().toLongLong();
            int updateEn  = obj.value("update_en").toInt(-1);
            qDebug() << "[iScreenDF][TCP] setFrequencyHz received:"
                     << "freq_hz =" << freqHz
                     << "update_en =" << updateEn;

            p->m_Frequency = static_cast<int>(freqHz);
            p->m_update_en = updateEn;

            if (db) {
                db->UpdateParameterField("Frequency", static_cast<qint64>(freqHz));
                db->UpdateParameterField("update_en", updateEn);
            }

            if (localDFclient) {
                emit rfsocParameterUpdated(p->m_Frequency, p->m_doaBwHz);
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }

        // ============ 14) setFcHz ============
        else if (menuID == "setFcHz") {
            qint64 fcHz = obj.value("fc_hz").toVariant().toLongLong();
            qDebug() << "[iScreenDF][TCP] setFcHz received:"
                     << "fc_hz =" << fcHz;

            if (localDFclient) {
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }

        // ============ RF AGC: setRfAgcEnable ============
        else if (menuID == "setRfAgcEnable") {
            int ch = obj.value("ch").toInt(-1);     // -1 = all
            bool enable = obj.value("enable").toBool(false);
            bool needAck = obj.value("needAck").toBool(false);

            qDebug() << "[iScreenDF][TCP] setRfAgcEnable received:"
                     << "ch =" << ch << "enable =" << enable << "needAck =" << needAck;

            // update local parameter state
            if (ch < 0) {
                p->m_rfAgcEnabled = enable;
                for (int i = 0; i < 5; ++i) p->m_rfAgcChEnabled[i] = enable;
                if (db) db->UpdateParameterField("rf_agc_enabled", enable ? 1 : 0);
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

        // ============ RF AGC: setRfAgcChannel ============
        else if (menuID == "setRfAgcChannel") {
            int ch = obj.value("ch").toInt(-1);
            double targetDb = obj.value("target_db").toDouble(-70.0);
            bool needAck = obj.value("needAck").toBool(false);

            qDebug() << "[iScreenDF][TCP] setRfAgcChannel received:"
                     << "ch =" << ch << "target_db =" << targetDb << "needAck =" << needAck;

            if (ch < 0) {
                for (int i = 0; i < 5; ++i) p->m_rfAgcTargetDb[i] = targetDb;
            } else if (ch >= 0 && ch < 5) {
                p->m_rfAgcTargetDb[ch] = targetDb;
            } else {
                qWarning() << "[iScreenDF][TCP] setRfAgcChannel: invalid ch =" << ch;
            }

            if (db) db->UpdateParameterField("rf_agc_target_db", targetDb);

            emit updateRfAgcTargetFromServer(ch, targetDb);

            if (localDFclient) {
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }

        // ============ 15) setDoaAlgorithm ============
        else if (menuID == "setDoaAlgorithm") {
            const QString algo = obj.value("algo").toString().trimmed();
            const bool needAck = obj.value("needAck").toBool(false);

            qDebug() << "[iScreenDF][TCP] setDoaAlgorithm received:"
                     << "algo =" << algo
                     << "needAck =" << needAck;

            if (!algo.isEmpty()) {
                p->m_doaAlgorithm = algo;
                if (db) db->UpdateParameterField("DoaAlgorithm", algo);
            }

            if (localDFclient) {
                emit updateDoaAlgorithmFromServer(p->m_doaAlgorithm);
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }

        // ============ 16) setUcaRadiusM ============
        else if (menuID == "setUcaRadiusM") {
            double radiusM = obj.value("radius_m").toDouble();
            bool needAck   = obj.value("needAck").toBool(false);

            qDebug() << "[iScreenDF][TCP] setUcaRadiusM received:"
                     << "radius_m =" << radiusM
                     << "needAck =" << needAck;

            p->m_ucaRadiusM = radiusM;
            if (db) db->UpdateParameterField("uca_radius_m", radiusM);

            if (localDFclient) {
                emit updateUcaRadiusFromServer(radiusM);
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }

        // ============ Scanner ATT: setScannerAttDb ============
        else if (menuID == "setScannerAttDb") {
            const double attDb = obj.value("att_db").toDouble(0.0);
            const bool needAck = obj.value("needAck").toBool(false);

            qDebug() << "[iScreenDF][TCP] setScannerAttDb received:"
                     << "att_db =" << attDb
                     << "needAck =" << needAck;

            p->m_scannerAttDb = attDb;  // ต้องมี field นี้ใน Parameter (double)
            // if (db) db->UpdateParameterField("scanner_att_db", attDb);

            if (localDFclient) {
                const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                localDFclient->sendLine(forwardLine, true);
            }
        }

        else {
            // unknown menuID -> จะ forward ก็ได้
            // if (localDFclient) {
            //     const QByteArray forwardLine = QJsonDocument(obj).toJson(QJsonDocument::Compact);
            //     localDFclient->sendLine(forwardLine, true);
            // }
        }

    } else {
        qDebug() << "[iScreenDF][TCP] Non-JSON message:" << message;

        if (localDFclient) {
            localDFclient->sendLine(message.toUtf8(), true);
        }
    }
}



void iScreenDF::onTcpClientConnected(const QHostAddress &addr,
                                     quint16 port)
{
    qDebug() << "[iScreenDF][TCP] Client connected from"
             << addr.toString() << ":" << port;

    // ถ้าต้องการ trigger อะไรตอนมี client ใหม่ เช่น ส่ง hello
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
        qWarning() << "[iScreenDF] sendRfAgcTargetDb: m_parameter empty/null";
        return;
    }

    Parameter *p = m_parameter.first();

    p->m_Frequency = frequencyHz;
    p->m_doaBwHz = bandwidthHz;
    const double offsetHz = bandwidthHz * 1.0; // 0.51
    updateReceiverParametersFreqOffsetBw((qint64)frequencyHz, offsetHz, (double)bandwidthHz);

    // if (chatServerDF) {
    QJsonObject obj;
    obj["menuID"]   = "updateReceiverFreqandbw";
    obj["Freq"]   = p->m_Frequency;
    obj["BW"]      = p->m_doaBwHz;
    obj["linkstatus"] = p->m_linkStatus;
    broadcastMessageServerandClient(obj);
    qDebug() << "[functionTcpServer] updateReceiverFreqandbw:" << obj;
    // }
}

static double clampDouble(double v, double lo, double hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void iScreenDF::updateReceiverParametersFreqOffsetBw(qint64 rfHz,double offsetHz,double bwHz)
{
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] updateReceiverParametersFreqOffsetBw: no parameter";
        return;
    }

    Parameter *p = m_parameter.first();

    // ================= FS constraint (ตรงกับ python) =================
    const double fsHz  = 240000.0;
    const double nyqHz = 0.49 * fsHz;

    offsetHz = clampDouble(offsetHz, -nyqHz, nyqHz);
    bwHz     = clampDouble(bwHz, 50.0, 0.45 * fsHz);

    // ================= Core logic =================
    // Fc = RF - offset (ไม่ปัดเศษ)
    const double rfHzD = (double)rfHz;
    double fcHz = rfHzD - offsetHz;
    if (fcHz < 0.0) fcHz = 0.0;

    emit rfsocParameterUpdated(p->m_Frequency, p->m_doaBwHz);
    // ✅ เก็บเป็น double (ต้องให้ชนิดตัวแปรใน Parameter รองรับ double)
    // p->m_Frequency      = fcHz;       // Fc (double)
    p->m_TargetOffsetHz = offsetHz;   // offset (double)
    p->m_doaBwHz        = bwHz;       // BW (double)

    db->UpdateParameterField("Frequency", static_cast<qint64>(p->m_Frequency));
    db->UpdateParameterField("TargetOffsetHz",p->m_TargetOffsetHz);
    db->UpdateParameterField("DoaBwHz",p->m_doaBwHz);

    qDebug() << "[iScreenDF][updateReceiverParametersFreqOffsetBw]"
             << "rf_hz="     << rfHzD
             << "fc_hz="     << p->m_Frequency
             << "offset_hz=" << p->m_TargetOffsetHz
             << "bw_hz="     << p->m_doaBwHz
             << "update_en=" << p->m_update_en;

    // ================= Send to server =================

    // 1) Fc (ส่งเป็น double)
    {
        QJsonObject o;
        o["menuID"]    = "setFrequencyHz";
        o["freq_hz"]   = (double)fcHz;   // ✅ ไม่ cast เป็น qint64
        o["update_en"] = p->m_update_en;
        o["needAck"]   = true;
        sendRfsocJsonLine(o, true);
    }

    // 2) Offset (double)
    {
        QJsonObject o;
        o["menuID"]    = "setDoaTargetOffsetHz";
        o["offset_hz"] = (double)p->m_TargetOffsetHz;
        o["needAck"]   = true;
        sendRfsocJsonLine(o, true);
    }

    // 3) BW (double)
    {
        QJsonObject o;
        o["menuID"]  = "setDoaBwHz";
        o["bw_hz"]   = (double)p->m_doaBwHz;
        o["needAck"] = true;
        sendRfsocJsonLine(o, true);
    }
}

void iScreenDF::sendSetDoaEnable(bool enable)
{
    Parameter *p = m_parameter.first();
    p->m_setDoaEnable  = enable;
    db->UpdateParameterField("setDoaEnable",p->m_setDoaEnable ? 1 : 0);
    QJsonObject obj;
    obj["menuID"]   = "setDoaEnable";
    obj["enable"]   = enable;
    obj["needAck"]  = true;
    sendRfsocJsonLine(obj, true);
}

void iScreenDF::sendSetSpectrumEnable(bool enable)
{
    Parameter *p = m_parameter.first();
    p->m_spectrumEnabled = enable;
    db->UpdateParameterField("spectrumEnabled", p->m_spectrumEnabled ? 1 : 0);
    emit rfsocDoaFftUpdated(p->m_setDoaEnable, p->m_spectrumEnabled);
    QJsonObject obj;
    obj["menuID"]   = "setSpectrumEnable";
    obj["enable"]   = enable;
    obj["needAck"]  = true;
    sendRfsocJsonLine(obj, true);
}

void iScreenDF::sendGateThDb(double v)
{
    if (v < -140.0) v = -140.0;
    if (v > 0.0)    v = 0.0;
    Parameter *p = m_parameter.first();
    qDebug() << "[iScreenDF] sendGateThDb from QML =" << v;

    p->m_doaPowerThresholdDb = static_cast<float>(v);

    m_blockUiSync = true;


    // update DB
    db->UpdateParameterField("DoaPowerThresholdDb",p->m_doaPowerThresholdDb);

    // ส่งไป server (TCP / WS)
    if (localDFclient) {
        QJsonObject obj;
        obj["menuID"] = "setDoaPowerThresholdDb";
        obj["th_db"]  = p->m_doaPowerThresholdDb;
        obj["needAck"] = true;

        localDFclient->sendLine(
            QJsonDocument(obj).toJson(QJsonDocument::Compact),
            true
            );
    }

    m_blockUiSync = false;
}

void iScreenDF::sendTxHz(double v)
{
    if (v < 0.2)  v = 0.2;
    if (v > 60.0) v = 60.0;

    qDebug() << "[iScreenDF] sendTxHz from QML =" << v;
    Parameter *p = m_parameter.first();
    m_blockUiSync = true;

    p->m_txHz =v;   // แนะนำให้ m_txHz เป็น double

    // update DB
    db->UpdateParameterField("TxHz", p->m_txHz);

    // forward to server
    if (localDFclient) {
        QJsonObject obj;
        obj["menuID"]  = "setTxHz";
        obj["hz"]      =  p->m_txHz;
        obj["needAck"] = true;
        localDFclient->sendLine(QJsonDocument(obj).toJson(QJsonDocument::Compact), true);
    }

    QTimer::singleShot(0, this, [this]() { m_blockUiSync = false; });
}
void iScreenDF::sendDoaAlgorithm(const QString &algo)
{
    Parameter *p = m_parameter.first();
    qDebug() << "[iScreenDF] sendDoaAlgorithm from QML =" << algo;

    m_blockUiSync = true;

    p->m_doaAlgorithm = algo;

    db->UpdateParameterField("DoaAlgorithm", algo);

    emit updateDoaAlgorithmFromServer(algo);   // sync UI

    if (localDFclient) {
        QJsonObject obj;
        obj["menuID"] = "setDoaAlgorithm";
        obj["algo"]   = algo;
        obj["needAck"] = true;
        localDFclient->sendLine(
            QJsonDocument(obj).toJson(QJsonDocument::Compact), true);
    }

    QTimer::singleShot(0, this, [this](){
        m_blockUiSync = false;
    });
}

void iScreenDF::sendUcaRadiusM(double radiusM)
{
    Parameter *p = m_parameter.first();
    // clamp
    if (radiusM < 0.01) radiusM = 0.01;
    if (radiusM > 10.0) radiusM = 10.0;

    qDebug() << "[iScreenDF] sendUcaRadiusM from QML =" << radiusM;

    m_blockUiSync = true;

    p->m_ucaRadiusM = radiusM;   // ✅ ให้ field เป็น double

    db->UpdateParameterField("uca_radius_m",  p->m_ucaRadiusM);

    // sync UI (เผื่อมีหลายหน้า/หลาย component)
    emit updateUcaRadiusFromServer(radiusM);

    // forward to server
    if (localDFclient) {
        QJsonObject obj;
        obj["menuID"]   = "setUcaRadiusM";
        obj["radius_m"] = radiusM;
        obj["needAck"]  = true;

        localDFclient->sendLine(
            QJsonDocument(obj).toJson(QJsonDocument::Compact),
            true
            );
    }

    // ปลดบล็อก (กัน loop)
    QTimer::singleShot(0, this, [this](){
        m_blockUiSync = false;
    });
}

static double clampRfAgcDb(double v)
{
    if (std::isnan(v) || std::isinf(v)) v = -60.0;
    if (v < -90.0) v = -90.0;
    if (v > -30.0) v = -30.0;
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

    for (int i = 0; i < 5; ++i) p->m_rfAgcTargetDb[i] = targetDb;

    db->UpdateParameterField("rf_agc_target_db", targetDb);

    if (localDFclient) {
        for (int ch = 0; ch < 5; ++ch) {
            QJsonObject obj;
            obj["menuID"]    = "setRfAgcChannel";
            obj["ch"]        = ch;
            obj["target_db"] = targetDb;
            obj["needAck"]   = true;
            qDebug() << "[iScreenDF] sendRfAgcTargetDb: setRfAgcChannel ch =" << ch;
            localDFclient->sendLine(QJsonDocument(obj).toJson(QJsonDocument::Compact), true);
        }
    } else {
        qWarning() << "[iScreenDF] sendRfAgcTargetAllDb: localDFclient is null";
    }

    for (int ch = 0; ch < 5; ++ch)
        emit updateRfAgcTargetFromServer(ch, targetDb);

    QTimer::singleShot(0, this, [this]() { m_blockUiSync = false; });
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

    // กัน loop (QML จะเช็ค krakenmapval.blockUiSync อยู่แล้ว)
    m_blockUiSync = true;

    // 1) update local parameter cache
    if (ch < 0) {
        // -1 => set all
        for (int i = 0; i < 5; ++i) p->m_rfAgcTargetDb[i] = targetDb;
    } else if (ch >= 0 && ch < 5) {
        p->m_rfAgcTargetDb[ch] = targetDb;
    } else {
        qWarning() << "[iScreenDF] sendRfAgcTargetDb: invalid ch =" << ch;
        m_blockUiSync = false;
        return;
    }

    // 2) update DB (ปรับชื่อ field ตามจริงของคุณ)
    // แนะนำ: ถ้า ch=-1 ให้ update ทั้ง 5 ช่อง
    // if (db) {
    //     if (ch < 0) {
    //         db->UpdateParameterField("rf_agc_target_db_0", p->m_rfAgcTargetDb[0]);
    //         db->UpdateParameterField("rf_agc_target_db_1", p->m_rfAgcTargetDb[1]);
    //         db->UpdateParameterField("rf_agc_target_db_2", p->m_rfAgcTargetDb[2]);
    //         db->UpdateParameterField("rf_agc_target_db_3", p->m_rfAgcTargetDb[3]);
    //         db->UpdateParameterField("rf_agc_target_db_4", p->m_rfAgcTargetDb[4]);
    //     } else {
    //         db->UpdateParameterField(QString("rf_agc_target_db_%1").arg(ch), p->m_rfAgcTargetDb[ch]);
    //     }
    // } else {
    //     qWarning() << "[iScreenDF] sendRfAgcTargetDb: db is null";
    // }
    // 3) forward to server (ใช้ menuID เดียวกับที่ server ส่งกลับมา)
    if (localDFclient) {
        QJsonObject obj;
        obj["menuID"]    = "setRfAgcChannel";
        obj["ch"]        = ch;        // -1 = all
        obj["target_db"] = targetDb;
        obj["needAck"]   = true;
        qDebug() << "[iScreenDF] sendRfAgcTargetDb: setRfAgcChannel ch =" << ch;
        localDFclient->sendLine(QJsonDocument(obj).toJson(QJsonDocument::Compact), true);
    } else {
        qWarning() << "[iScreenDF] sendRfAgcTargetDb: localDFclient is null";
    }

    // 4) push กลับ QML ให้ UI sync (สำคัญมาก)
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

    // กัน loop
    m_blockUiSync = true;

    // update local cache
    if (ch < 0) {
        p->m_rfAgcEnabled = enable;
        for (int i = 0; i < 5; ++i) p->m_rfAgcChEnabled[i] = enable;
    } else if (ch >= 0 && ch < 5) {
        p->m_rfAgcChEnabled[ch] = enable;
    } else {
        qWarning() << "[iScreenDF] sendRfAgcEnable: invalid ch =" << ch;
        m_blockUiSync = false;
        return;
    }

    // update DB (ถ้าต้องการ)
    // if (db) {
    //     if (ch < 0) {
    //         db->UpdateParameterField("rf_agc_enabled", enable ? 1 : 0);
    //         for (int i = 0; i < 5; ++i)
    //             db->UpdateParameterField(QString("rf_agc_ch_enabled_%1").arg(i), enable ? 1 : 0);
    //     } else {
    //         db->UpdateParameterField(QString("rf_agc_ch_enabled_%1").arg(ch), enable ? 1 : 0);
    //     }
    // }
    db->UpdateParameterField("rf_agc_enabled", enable ? 1 : 0);
    // forward to server
    if (localDFclient) {
        QJsonObject obj;
        obj["menuID"]  = "setRfAgcEnable";
        obj["ch"]      = ch;         // ✅ สำหรับ enable: -1 = all ใช้ได้
        obj["enable"]  = enable;
        obj["needAck"] = true;
        localDFclient->sendLine(QJsonDocument(obj).toJson(QJsonDocument::Compact), true);
    } else {
        qWarning() << "[iScreenDF] sendRfAgcEnable: localDFclient is null";
    }

    // push กลับ QML ให้ sync
    emit updateRfAgcEnableFromServer(ch, enable);

    QTimer::singleShot(0, this, [this]() {
        m_blockUiSync = false;
    });
}
void iScreenDF::setLinkStatus(bool linkStatus)
{
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] sendRfAgcTargetDb: m_parameter empty/null";
        return;
    }

    Parameter *p = m_parameter.first();

    p->m_linkStatus = linkStatus;

    qDebug() << "[iScreenDF] setLinkStatusFromQml =" << linkStatus;
    db->UpdateParameterField("linkstatus", linkStatus ? 1 : 0);
}
// -----------------------------
// sanitizePathPart
// -----------------------------
QString iScreenDF::sanitizePathPart(QString s)
{
    s = s.trimmed();
    s.replace(QRegularExpression(R"(\s+)"), "_");
    s.replace(QRegularExpression(R"([^A-Za-z0-9_\-\.])"), "_");
    if (s.isEmpty()) s = "NA";
    return s;
}

// -----------------------------
// freqToFolder: 120000000 -> "120_000MHz"
// -----------------------------
QString iScreenDF::freqToFolder(double freqHz)
{
    if (!qIsFinite(freqHz) || freqHz <= 0) return "0MHz";

    const double mhz = freqHz / 1e6;
    QString s = QString::number(mhz, 'f', 3); // "120.000"
    s.replace('.', '_');                      // "120_000"
    return s + "MHz";
}

// -----------------------------
// dateToFolder: "22 Jan 2026" -> "2026-01-22" (fallback from updatedMs)
// -----------------------------
QString iScreenDF::dateToFolder(const QString &dateStr, double ms)
{
    QDate d = QDate::fromString(dateStr.trimmed(), "dd MMM yyyy");

    if (!d.isValid()) {
        const qint64 msi = static_cast<qint64>(ms);
        if (msi > 0) d = QDateTime::fromMSecsSinceEpoch(msi).date();
    }
    if (!d.isValid()) d = QDate::currentDate();

    return d.toString("yyyy-MM-dd");
}

// -----------------------------
// timeToFolder: "14:12:49" -> "14-12-49" (fallback from updatedMs)
// -----------------------------
QString iScreenDF::timeToFolder(const QString &timeStr, double ms)
{
    QTime t = QTime::fromString(timeStr.trimmed(), "HH:mm:ss");

    if (!t.isValid()) {
        const qint64 msi = static_cast<qint64>(ms);
        if (msi > 0) t = QDateTime::fromMSecsSinceEpoch(msi).time();
    }
    if (!t.isValid()) t = QTime::currentTime();

    return t.toString("HH-mm-ss");
}
// -----------------------------
// buildDailyCsvPath
// ✅ 1 ไฟล์ต่อ freq+day
// ตัวอย่าง:
// /var/log/iScreenDF/120_000MHz/2026-01-22/120_000MHz_2026-01-22.csv
// -----------------------------
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
// -----------------------------
// ensureDir
// -----------------------------
bool iScreenDF::ensureDir(const QString &dirPath)
{
    QDir dir;
    return dir.mkpath(dirPath);
}

// -----------------------------
// updateActiveCsvIfNeeded
// ✅ เปลี่ยนความถี่ หรือ ข้ามวัน -> เปลี่ยนไฟล์ใหม่
// -----------------------------
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
        return; // ยังเป็นไฟล์เดิม
    }

    // เปลี่ยนไฟล์ใหม่
    m_activeFreqFolder = newFreqFolder;
    m_activeDayFolder  = newDayFolder;
    m_activeCsvPath    = newPath;

    // หมายเหตุ: ไม่ต้อง reset dedupe keys ก็ได้
    // แต่ถ้าต้องการกัน "key เดิมจากไฟล์เก่า" มากระทบไฟล์ใหม่:
    m_lastTxSeenKey.clear();
    m_lastTxWrittenKey.clear();

    // สร้างโฟลเดอร์ล่วงหน้า (optional)
    const QFileInfo fi(m_activeCsvPath);
    ensureDir(fi.absolutePath());
}
// -----------------------------
// appendTxCsvRow (creates header if new file)
// -----------------------------
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

    // if (newFile) {
    //     out << "lat,lon,rms_m,freqHz,date,time,updatedMs,mgrs\n";
    // }

    // out << latStr << ","
    //     << lonStr << ","
    //     << rmsStr << ","
    //     << freqStr << ","
    //     << dateStr << ","
    //     << timeStr << ","
    //     << updatedMsStr << ","
    //     << mgrs
    //     << "\n";
    if (newFile) {
        out << "rms_m,freqHz,date,time,updatedMs,mgrs\n";
    }

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
    // ✅ format lat/lon = 6 decimals
    const QString latStr = QString::number(lat, 'f', 6);
    const QString lonStr = QString::number(lon, 'f', 6);

    // ✅ normalize others
    const QString rmsStr   = QString::number(rms_m,  'f', 2);
    const QString freqStr  = QString::number(freqHz, 'f', 0);
    const QString upMsStr  = QString::number(updatedMs, 'f', 0);

    // ✅ KEY ไม่รวม updatedMs (เพื่อให้ 9489 กับ 9496 ถือว่า "ซ้ำ")
    const QString key =
        latStr + "," + lonStr +
        "|rms=" + rmsStr +
        "|f=" + freqStr +
        "|mgrs=" + mgrs;

    // ✅ เปลี่ยนไฟล์เมื่อ freq/day เปลี่ยน
    updateActiveCsvIfNeeded(freqHz, dateStr, updatedMs);

    // ------------------------------------------------------------
    // CASE 1: key ใหม่ (เข้ามาครั้งแรก) -> ยังไม่เขียน
    // ------------------------------------------------------------
    if (key != m_lastTxSeenKey) {
        m_lastTxSeenKey = key;
        return;
    }

    // ------------------------------------------------------------
    // CASE 2: key ซ้ำ (เข้ามาครั้งที่ 2+)
    // แต่ถ้าเคยเขียนไปแล้ว -> ไม่เขียนซ้ำ
    // ------------------------------------------------------------
    if (key == m_lastTxWrittenKey) {
        return;
    }

    // ------------------------------------------------------------
    // CASE 3: ซ้ำจริง และยังไม่เคยเขียน -> APPEND 1 แถว
    // ------------------------------------------------------------
    m_lastTxWrittenKey = key;

    const QString csvPath = m_activeCsvPath;

    const bool ok = appendTxCsvRow(csvPath,
                                   latStr, lonStr, rmsStr, freqStr,
                                   dateStr, timeStr, upMsStr, mgrs);

    // ✅ optional debug log: เฉพาะตอนเขียนจริง
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


// void ::updateReceiverParametersFreqOffsetBw(qint64 rfHz, double offsetHz, double bwHz)
// {
//     if (m_parameter.isEmpty() || !m_parameter.first()) {
//         qWarning() << "[iScreenDF] updateReceiverParametersFreqOffsetBw: no parameter";
//         return;
//     }

//     Parameter *p = m_parameter.first();
//     if (!p) {
//         qWarning() << "[iScreenDF] updateReceiverParametersFreqOffsetBw: no parameter ptr";
//         return;
//     }

//     // ใช้ FS ของ server (ตอนนี้ใน python คือ FS=240000)
//     const double fsHz  = 240000.0;
//     const double nyqHz = 0.49 * fsHz;

//     // clamp offset ให้อยู่ใน [-nyq, +nyq]
//     offsetHz = clampDouble(offsetHz, -nyqHz, nyqHz);

//     // clamp bw: 50 .. 0.45*FS ตาม python
//     bwHz = clampDouble(bwHz, 50.0, 0.45 * fsHz);

//     p->m_Frequency      = (int)rfHz;              // เก็บ RF
//     p->m_TargetOffsetHz = (int)qRound64(offsetHz);
//     p->m_doaBwHz        = (int)qRound64(bwHz);

//     qDebug() << "[iScreenDF][updateReceiverParametersFreqOffsetBw]"
//              << "rf_hz="     << p->m_Frequency
//              << "offset_hz=" << p->m_TargetOffsetHz
//              << "bw_hz="     << p->m_doaBwHz
//              << "update_en=" << p->m_update_en;

//     // 1) RF (Fc) -> server ใช้เป็น FREQ_HZ
//     {
//         QJsonObject o;
//         o["menuID"]    = "setFrequencyHz";
//         o["freq_hz"]   = (qint64)p->m_Frequency;
//         o["update_en"] = p->m_update_en;
//         o["needAck"]   = true;
//         sendRfsocJsonLine(o, true);
//     }

//     // 2) Offset
//     {
//         QJsonObject o;
//         o["menuID"]    = "setDoaTargetOffsetHz";
//         o["offset_hz"] = (double)p->m_TargetOffsetHz;
//         o["needAck"]   = true;
//         sendRfsocJsonLine(o, true);
//     }

//     // 3) BW
//     {
//         QJsonObject o;
//         o["menuID"]  = "setDoaBwHz";
//         o["bw_hz"]   = (double)p->m_doaBwHz;
//         o["needAck"] = true;
//         sendRfsocJsonLine(o, true);
//     }
// }

// void iScreenDF::updateReceiverParametersFreqandbw(int frequencyHz, int bandwidthHz)
// {
//     if (m_parameter.isEmpty() || !m_parameter.first()) {
//         qWarning() << "[iScreenDF] updateReceiverParametersFreqandbw: no parameter";
//         return;
//     }

//     Parameter *p = m_parameter.first();
//     if (!p) {
//         qWarning() << "[iScreenDF] updateReceiverParametersFreqandbw: no parameter ptr";
//         return;
//     }

//     // ------------------------------------------------------
//     // INPUT:
//     //   frequencyHz  = "RF frequency" ที่ user ใส่มา (Hz)
//     //   bandwidthHz  = BW ที่ user ใส่มา (Hz)
//     //
//     // REQUIREMENT:
//     //   p->m_TargetOffsetHz = p->m_doaBwHz
//     //   p->m_Frequency      = m_Frequency - m_TargetOffsetHz
//     // ------------------------------------------------------

//     // เก็บค่าที่ user ใส่เป็น "m_Frequency" (RF) ก่อน
//     p->m_Frequency = frequencyHz;

//     // BW -> ใช้เป็นทั้ง doaBw และ offset
//     p->m_doaBwHz        = bandwidthHz;
//     p->m_TargetOffsetHz = p->m_doaBwHz;

//     // ป้องกัน Fc กลายเป็นค่าติดลบ
//     qint64 fc = (qint64)p->m_Frequency - (qint64)p->m_TargetOffsetHz;
//     if (fc < 0) fc = 0;

//     // Fc ที่จะส่งให้ server
//     p->m_Frequency = (int)fc;

//     qDebug() << "[iScreenDF][updateReceiverParametersFreqandbw]"
//              << "send fc_hz=" << p->m_Frequency
//              << "offset_hz="  << p->m_TargetOffsetHz
//              << "bw_hz="      << p->m_doaBwHz
//              << "update_en="  << p->m_update_en;

//     // 1) Fc
//     {
//         QJsonObject o;
//         o["menuID"]    = "setFrequencyHz";
//         o["freq_hz"]   = (qint64)p->m_Frequency;
//         o["update_en"] = p->m_update_en;
//         o["needAck"]   = true;
//         sendRfsocJsonLine(o, true);
//     }

//     // 2) Offset (เท่ากับ BW)
//     {
//         QJsonObject o;
//         o["menuID"]    = "setDoaTargetOffsetHz";
//         o["offset_hz"] = (double)p->m_TargetOffsetHz;
//         o["needAck"]   = true;
//         sendRfsocJsonLine(o, true);
//     }

//     // 3) BW
//     {
//         QJsonObject o;
//         o["menuID"]  = "setDoaBwHz";
//         o["bw_hz"]   = (double)p->m_doaBwHz;
//         o["needAck"] = true;
//         sendRfsocJsonLine(o, true);
//     }
// }

