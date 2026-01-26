#include "alsarecconfigmanager.h"
#include <QSettings>
#include <QProcess>
#include <QTcpSocket>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

AlsaRecConfigManager::AlsaRecConfigManager(QObject *parent)
    : QObject(parent)
{
    RecorderSocketServer = new ChatServer(8072);
    connect(RecorderSocketServer,&ChatServer::recLogging,this,&AlsaRecConfigManager::recLogging);;
    autoAnnounceTimer = new QTimer(this);
    loopAnnounceTimer = new QTimer(this);
    loopCheckServerAlive = new QTimer(this);

    connect(loopAnnounceTimer,&QTimer::timeout,this,&AlsaRecConfigManager::autoAnnounce);
    connect(loopCheckServerAlive,&QTimer::timeout,this,&AlsaRecConfigManager::checkServerAlive);
    loopCheckServerAlive->start(60*1000*1);
    loopAnnounceTimer->start(10*1000);

}
// AlsaRecConfigManager.cpp
//AlsaRecConfigManager::AlsaRecConfigManager(ChatServer *server, QObject *parent)
//    : QObject(parent),
//      RecorderSocketServer(server)
//{
//    Q_ASSERT(RecorderSocketServer);

//    connect(RecorderSocketServer, &ChatServer::recLogging,
//            this, &AlsaRecConfigManager::recLogging);

//    loopCheckServerAlive = new QTimer(this);
//    loopAnnounceTimer    = new QTimer(this);

//    connect(loopAnnounceTimer, &QTimer::timeout, this, &AlsaRecConfigManager::autoAnnounce);
//    connect(loopCheckServerAlive, &QTimer::timeout, this, &AlsaRecConfigManager::checkServerAlive);

//    loopCheckServerAlive->start(60*1000);
//    loopAnnounceTimer->start(10*1000);
//}

void AlsaRecConfigManager::handleApplyRecSettings(const QJsonObject &obj)
{
    qDebug() << "handleApplyRecSettings" << obj;

    QRegularExpression re("^REC_(\\d+)$");

    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const QString key = it.key();

        if (key == "menuID")
            continue;

        QRegularExpressionMatch match = re.match(key);
        if (!match.hasMatch()) {
            qDebug() << "[handleApplyRecSettings] ignore non-rec key:" << key;
            continue;
        }

        if (!it.value().isObject()) {
            qWarning() << "[handleApplyRecSettings] invalid rec object (not object):" << key;
            continue;
        }

        QJsonObject recObj = it.value().toObject();

        AlsaRecConfig config;
        config.recordID = match.captured(1).toInt();

        config.alsa_dev = recObj.value("alsa_dev").toString().trimmed();
        config.client_as_ip = recObj.value("client_as_ip").toString().trimmed();

        double f = 0.0;
        if (recObj.value("client_as_freq").isDouble())
            f = recObj.value("client_as_freq").toDouble();
        else
            f = recObj.value("client_as_freq").toString().toDouble();

        if (f > 1000000.0)
            f /= 1000000.0;

        config.client_as_freq = f;

        config.rtsp_server_ip = recObj.value("rtsp_server_ip").toString().trimmed();
        config.rtsp_server_port = recObj.value("rtsp_server_port").toInt();
        config.rtsp_server_uri = recObj.value("rtsp_server_uri").toString().trimmed();
        config.service = recObj.value("service").toString().trimmed();
        config.enable = recObj.value("enable").toBool();

        setConfig(key, config);

        saveConfig(ALSARECCONF, key);
    }
}


void AlsaRecConfigManager::autoAnnounce()
{
    pendingAnnounceIDs.clear();

    for (auto it = m_configs.begin(); it != m_configs.end(); ++it) {
        if (it.value().status == "READY")
        {
            QString key = it.key(); // e.g. REC_3
            bool ok = false;
            int id = key.mid(4).toInt(&ok);
            if (ok) {
                if(it.value().enable)
                    pendingAnnounceIDs.append(id);
            }
        }
        else if (it.value().status == "TEARDOWN")
        {
            if (it.value().enable == false)
                stopService(it.value().service);
            else
                restartService(it.value().service);
        }
    }

    if (!pendingAnnounceIDs.isEmpty()) {
        connect(autoAnnounceTimer, &QTimer::timeout,
                this, &AlsaRecConfigManager::processNextAnnounce,
                Qt::UniqueConnection);
        autoAnnounceTimer->start(5 * 1000);
        // processNextAnnounce();
        loopAnnounceTimer->start(60*1000);
    } else {
        autoAnnounceTimer->stop();
        loopAnnounceTimer->start(10*1000);
    }
}

double AlsaRecConfigManager::getFrequency(uint8_t iGateID)
{
    QString sectionKey = QString("REC_%1").arg(iGateID);

    if (m_configs.contains(sectionKey)) {
        return m_configs[sectionKey].client_as_freq;
    }

    return 0.0; // or some sentinel value indicating "not found"
}

void AlsaRecConfigManager::processNextAnnounce()
{
    if (pendingAnnounceIDs.isEmpty()) {
        autoAnnounceTimer->stop();
        qDebug() << "autoAnnounce done. No more READY states.";
        return;
    }

    int iGateID = pendingAnnounceIDs.takeFirst();

    qDebug() << "Sending announce for iGateID:" << iGateID;
    double freq = getFrequency(iGateID);
    if (RecorderSocketServer) {
        RecorderSocketServer->sendSquelchStatus(iGateID, false, false, false, "announce", freq);
        qDebug() << "AlsaRecConfigManager" << "Sending announce for iGateID:" << iGateID << "Freq" << freq;
    }

    // if (pendingAnnounceIDs.isEmpty()) {
    //     this->autoAnnounce();
    // }
}
void AlsaRecConfigManager::teardown(int recID)
{

    qDebug() << "Sending teardown for recID:" << recID;
    double freq = getFrequency(recID);
    if (RecorderSocketServer) {
        RecorderSocketServer->sendSquelchStatus(recID, false, false, false, "", freq);
        // RecorderSocketServer->sendSquelchStatus(recID, false, false, false, "teardown", freq); //Main Recorder FAILURE!
    }
}

//bool AlsaRecConfigManager::loadConfig(const QString &filePath)
//{
//    QFileInfo fi(filePath);
//    qDebug() << "[loadConfig] filePath=" << filePath
//             << "exists=" << fi.exists()
//             << "isFile=" << fi.isFile()
//             << "readable=" << fi.isReadable()
//             << "size=" << fi.size();

//    QSettings settings(filePath, QSettings::IniFormat);
//    qDebug() << "[loadConfig] settingsStatus=" << settings.status();
//    qDebug() << "[loadConfig] childGroups=" << settings.childGroups();

//    m_configs.clear();

//    const auto groups = settings.childGroups();
//    for (const QString &group : groups) {
//        settings.beginGroup(group);

//        AlsaRecConfig config;
//        config.alsa_dev          = settings.value("alsa_dev").toString();
//        config.client_as_ip      = settings.value("client_as_ip").toString();
//        config.client_as_freq    = settings.value("client_as_freq").toDouble();
//        config.rtsp_server_ip    = settings.value("rtsp_server_ip").toString();
//        config.rtsp_server_port  = settings.value("rtsp_server_port").toInt();
//        config.rtsp_server_uri   = settings.value("rtsp_server_uri").toString();
//        config.service           = settings.value("service").toString();
//        config.enable            = settings.value("enable").toString().compare("true", Qt::CaseInsensitive) == 0;

//        qDebug() << "[loadConfig] group=" << group
//                 << "enable=" << config.enable
//                 << "service=" << config.service
//                 << "freq=" << config.client_as_freq;

//        m_configs[group] = config;
//        settings.endGroup();
//    }

//    qDebug() << "[loadConfig] loadedKeys=" << m_configs.keys();
//    return !m_configs.isEmpty();
//}
bool AlsaRecConfigManager::loadConfig(const QString &filePath)
{
    QSettings settings(filePath, QSettings::IniFormat);

    m_configs.clear();

    foreach (const QString &group, settings.childGroups()) {
        settings.beginGroup(group);

        AlsaRecConfig config;
        config.alsa_dev = settings.value("alsa_dev").toString();
        config.client_as_ip = settings.value("client_as_ip").toString();
        config.client_as_freq = settings.value("client_as_freq").toDouble();
        config.rtsp_server_ip = settings.value("rtsp_server_ip").toString();
        config.rtsp_server_port = settings.value("rtsp_server_port").toInt();
        config.rtsp_server_uri = settings.value("rtsp_server_uri").toString();
        config.service = settings.value("service").toString();
        config.enable = settings.value("enable").toString().compare("true", Qt::CaseInsensitive) == 0;

        m_configs[group] = config;

        settings.endGroup();
    }

    return true;
}
void AlsaRecConfigManager::applyAllConfigs()
{
    for (const QString &key : m_configs.keys()) {
        AlsaRecConfig &config = m_configs[key];

        int recID = 0;
        QRegularExpression re("REC_(\\d+)$");
        QRegularExpressionMatch match = re.match(key);
        if (match.hasMatch()) {
            recID = match.captured(1).toInt();
        }

        config.recordID = recID;

        if (config.enable) {
            startService(config.service);
        }
    }

    autoAnnounce();
}
//bool AlsaRecConfigManager::loadConfig(const QString &filePath)
//{
//    qDebug() << "AlsaRecConfigManager:" << filePath;

//    QSettings settings(filePath, QSettings::IniFormat);

//    m_configs.clear();

//    foreach (const QString &group, settings.childGroups()) {
//        settings.beginGroup(group);

//        AlsaRecConfig config;
//        config.alsa_dev = settings.value("alsa_dev").toString();
//        config.client_as_ip = settings.value("client_as_ip").toString();
//        config.client_as_freq = settings.value("client_as_freq").toDouble();
//        config.rtsp_server_ip = settings.value("rtsp_server_ip").toString();
//        config.rtsp_server_port = settings.value("rtsp_server_port").toInt();
//        config.rtsp_server_uri = settings.value("rtsp_server_uri").toString();
//        config.service = settings.value("service").toString();
//        config.enable = settings.value("enable").toString().compare("true", Qt::CaseInsensitive) == 0;

//        m_configs[group] = config;

//        settings.endGroup();
//    }

//    return true;
//}

bool AlsaRecConfigManager::saveConfig(const QString &filePath, const QString &targetKey)
{
    if (!m_configs.contains(targetKey))
        return false;

    QSettings settings(filePath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");

    settings.beginGroup(targetKey);

    const AlsaRecConfig &config = m_configs[targetKey];

    settings.setValue("alsa_dev", config.alsa_dev);
    settings.setValue("client_as_ip", config.client_as_ip);

    settings.setValue("client_as_freq", QString::number(config.client_as_freq, 'f', 4));

    settings.setValue("rtsp_server_ip", config.rtsp_server_ip);
    settings.setValue("rtsp_server_port", config.rtsp_server_port);
    settings.setValue("rtsp_server_uri", config.rtsp_server_uri);
    settings.setValue("service", config.service);
    settings.setValue("enable", config.enable ? "true" : "false");

    settings.endGroup();
    settings.sync();

    return (settings.status() == QSettings::NoError);
}

void AlsaRecConfigManager::updateClientAsIPForAllConfigs(const QString &newIP)
{
    bool changed = false;

    for (auto it = m_configs.begin(); it != m_configs.end(); ++it) {
        AlsaRecConfig &config = it.value();
        if (config.client_as_ip != newIP) {
            qDebug() << "Updating client_as_ip in" << it.key()
            << "from" << config.client_as_ip << "to" << newIP;
            config.client_as_ip = newIP;
            changed = true;

            if (config.enable)
                restartService(config.service); // restart only active configs
        }
    }

    if (changed) {
        saveConfig(ALSARECCONF);
        qDebug() << "[AlsaRecConfigManager] All client_as_ip updated and config saved.";
    } else {
        qDebug() << "[AlsaRecConfigManager] No changes to client_as_ip. All values are already" << newIP;
    }
}

void AlsaRecConfigManager::updateRtspUriForConfigs(int recID, const QString &newUri)
{
    auto updateOne = [&](int id) {
        QString key = QString("REC_%1").arg(id);

        if (!m_configs.contains(key)) {
            qWarning() << "[updateRtspUriForConfigs] Config not found for" << key;
            return;
        }

        AlsaRecConfig &config = m_configs[key];

        if (config.rtsp_server_uri != newUri) {
            qDebug() << "[updateRtspUriForConfigs] Updating rtsp_server_uri for" << key
                     << "from" << config.rtsp_server_uri << "to" << newUri;

            config.rtsp_server_uri = newUri;

            if (config.enable)
                restartService(config.service);

            saveConfig(ALSARECCONF, key);
        } else {
            qDebug() << "[updateRtspUriForConfigs] No change for" << key;
        }
    };

    updateOne(recID);
    updateOne(recID + 4);
}


bool AlsaRecConfigManager::saveConfig(const QString &filePath)
{
    QSettings settings(filePath, QSettings::IniFormat);

    settings.clear();

    for (auto it = m_configs.begin(); it != m_configs.end(); ++it) {
        settings.beginGroup(it.key());

        const AlsaRecConfig &config = it.value();
        settings.setValue("alsa_dev", config.alsa_dev);
        settings.setValue("client_as_ip", config.client_as_ip);
//        settings.setValue("client_as_freq", config.client_as_freq);
        settings.setValue("client_as_freq",QString::number(config.client_as_freq, 'f', 1));

        settings.setValue("rtsp_server_ip", config.rtsp_server_ip);
        settings.setValue("rtsp_server_port", config.rtsp_server_port);
        settings.setValue("rtsp_server_uri", config.rtsp_server_uri);
        settings.setValue("service", config.service);
        settings.setValue("enable", config.enable ? "true" : "false");

        settings.endGroup();
    }

    settings.sync();
    return true;
}

QMap<QString, AlsaRecConfig> AlsaRecConfigManager::getConfigs() const
{
    return m_configs;
}
QString AlsaRecConfigManager::getState(int iGateID)
{
    if (iGateID < 1 || iGateID > 8)
        return QString();

    QString sectionKey = QString("REC_%1").arg(iGateID);

    if (m_configs.contains(sectionKey)) {
        return m_configs.value(sectionKey).status;
    }

    return QString();
}
void AlsaRecConfigManager::sendSquelchStatus(int softPhoneID, bool pttOn, bool sqlOn, bool callState, double freq)
{
    qDebug() << "AlsaRecConfigManager_sendSquelchStatus:" << softPhoneID << pttOn << sqlOn << callState << freq;//<< message;
    auto processDevice = [&](int id) {
        QString sectionKey = QString("REC_%1").arg(id);
        if (!m_configs.contains(sectionKey)) return;

        AlsaRecConfig &cfg = m_configs[sectionKey];
        QString state = cfg.status;
        qDebug() << "AlsaRecConfigManager_sendSquelchStatus_state:" << state;

        bool sql = sqlOn;
        bool ptt = pttOn;
        double client_as_freq = freq;
        qDebug() << "sql:" << sql << "ptt:" << ptt << "client_as_freq:" << client_as_freq;

        if (cfg.enable) {
            sql = sqlOn;
            ptt = pttOn;
        }

        if (freq == 0)
            client_as_freq = cfg.client_as_freq;
        else if (cfg.client_as_freq != freq) {
            cfg.client_as_freq = freq;
            saveConfig(ALSARECCONF, sectionKey);
        }

        // Cancel any pending PAUSE if we are going to send RECORD
        if (state == "PAUSE" && (sql || ptt)) {
            if (pauseTimers.contains(id)) {
                pauseTimers[id]->stop();
                pauseTimers[id]->deleteLater();
                pauseTimers.remove(id);
            }
            qDebug() << "RecorderSocketServer_PAUSE:";//<< message;
            RecorderSocketServer->sendSquelchStatus(id, ptt, sql, callState, state, client_as_freq);
        }
        // Delay PAUSE only if still RECORD and not active
        else if (state == "RECORD" && (!sql && !ptt)) {
            if (!pauseTimers.contains(id)) {
                QTimer *pauseTimer = new QTimer(this);
                pauseTimer->setSingleShot(true);
                pauseTimer->setInterval(200);
                connect(pauseTimer, &QTimer::timeout, this, [=]() {
                    // Re-check state before sending
                    if (m_configs.contains(sectionKey) && m_configs[sectionKey].status == "RECORD") {
                        qDebug() << "RecorderSocketServer_RECORD:";//<< message;
                        RecorderSocketServer->sendSquelchStatus(id, ptt, sql, callState, state, client_as_freq);
                    }
                    pauseTimers.remove(id);
                    pauseTimer->deleteLater();
                });
                pauseTimers[id] = pauseTimer;
                pauseTimer->start();
            }
        }
    };

    processDevice(softPhoneID);
    processDevice(softPhoneID + 4);
}

//void AlsaRecConfigManager::sendSquelchStatus(int softPhoneID, bool pttOn, bool sqlOn, bool callState, double freqHz)
//{
//    qDebug() << "AlsaRecConfigManager_sendSquelchStatus--->" << softPhoneID << pttOn << sqlOn << callState << freqHz;

//    auto processDevice = [&](int id) {
//        QString sectionKey = QString("REC_%1").arg(id);
//        if (!m_configs.contains(sectionKey)) return;

//        AlsaRecConfig &cfg = m_configs[sectionKey];

//        if (!cfg.enable) return; // ปิดไว้ก็ไม่ต้องสั่ง

//        // ✅ freqHz ถ้า 0 ใช้ค่าใน config
//        double useFreqHz = (freqHz > 0) ? freqHz : cfg.client_as_freq;

//        // ✅ อัปเดต config ถ้า freq เปลี่ยน (Hz)
//        if (freqHz > 0 && cfg.client_as_freq != freqHz) {
//            cfg.client_as_freq = freqHz;
//            saveConfig(ALSARECCONF, sectionKey);
//        }

//        const bool active = (sqlOn || pttOn);

//        // ✅ target state ตามธรรมชาติ
//        const QString targetState = active ? "RECORD" : "PAUSE";

//        qDebug() << "sectionKey" << sectionKey
//                 << "cfg.status=" << cfg.status
//                 << "targetState=" << targetState
//                 << "useFreqHz=" << useFreqHz;

//        // ====== ON -> RECORD ทันที ======
//        if (active) {
//            // ยกเลิก timer pause ที่ค้างไว้
//            if (pauseTimers.contains(id)) {
//                pauseTimers[id]->stop();
//                pauseTimers[id]->deleteLater();
//                pauseTimers.remove(id);
//            }

//            RecorderSocketServer->sendSquelchStatus(id, pttOn, sqlOn, callState, "RECORD", useFreqHz);
//            return;
//        }

//        // ====== OFF -> PAUSE แบบหน่วง ======
//        if (!pauseTimers.contains(id)) {
//            QTimer *t = new QTimer(this);
//            t->setSingleShot(true);
//            t->setInterval(200);

//            connect(t, &QTimer::timeout, this, [=]() {
//                // ยัง OFF อยู่ค่อย PAUSE
//                RecorderSocketServer->sendSquelchStatus(id, false, false, callState, "PAUSE", useFreqHz);
//                pauseTimers.remove(id);
//                t->deleteLater();
//            });

//            pauseTimers[id] = t;
//            t->start();
//        }
//    };

//    processDevice(softPhoneID);
//    processDevice(softPhoneID + 4); // ✅ แก้จาก +1 เป็น +4
//}



void AlsaRecConfigManager::recLogging(int softPhoneID, int recorderID, QString recState, QString message)
{
     qDebug() << "recLogging:" << message;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << parseError.errorString();
        return;
    }

    if (!doc.isObject()) {
        qWarning() << "Invalid JSON object";
        return;
    }

    QJsonObject obj = doc.object();

    int iGateID = obj.value("iGateID").toInt(-1);
    QString state = obj.value("state").toString();

    if (iGateID < 1 || iGateID > 8) {
        // qWarning() << "Invalid iGateID:" << iGateID;
        return;
    }

    QString sectionKey = QString("REC_%1").arg(iGateID);
    qDebug() << "sectionKey" << sectionKey << state;
    if (m_configs.contains(sectionKey)) {
        AlsaRecConfig config = m_configs[sectionKey];
        config.status = state;
        if ((state != "SETUP") && (state != "ANNOUNCE"))
            config.lastMessage = QDateTime::currentSecsSinceEpoch();
        m_configs[sectionKey] = config;

        if ((pendingAnnounceIDs.isEmpty()) && (state == "READY"))
        {

        }
        else if (config.status == "TEARDOWN")
        {
            if (config.enable == false)
                stopService(config.service);
            else
                restartService(config.service);
        }
        // qDebug() << "Updated config for section:" << sectionKey
        //          << "new status:" << state;

        // optional: emit signal if you want UI updates
        emit sendMessageToWeb(message);
    } else {
        qWarning() << "No config found for section:" << sectionKey;
    }
}


void AlsaRecConfigManager::setConfig(const QString &section, const AlsaRecConfig &config)
{
    qDebug() << "AlsaRecConfigManager setConfig:" << &config << section;
    if (m_configs.contains(section)) {
        if (m_configs[section] != config) {
            m_configs[section] = config;
            emit configChanged(section);
            qDebug() << "restartService_config:" << config.enable;
            if(config.enable)
                restartService(config.service);
            else
            {
                teardown(config.recordID);
                // stopService(config.service);
            }
        }
    } else {
        m_configs[section] = config;
        emit configChanged(section);
        qDebug() << "restartService_config2:" << config.enable;
        if(config.enable)
            restartService(config.service);
        else
        {
            teardown(config.recordID);
            // stopService(config.service);
        }
    }
}

bool AlsaRecConfigManager::isPortOpen(const QString &ip, int port)
{
    QTcpSocket socket;
    socket.connectToHost(ip, port);
    return socket.waitForConnected(1000);
}

bool AlsaRecConfigManager::isServiceActive(const QString &serviceName)
{
    QProcess process;
    process.start("systemctl", QStringList() << "is-active" << serviceName);
    process.waitForFinished();
    QString output = process.readAllStandardOutput().trimmed();
    return (output == "active");
}

void AlsaRecConfigManager::startService(const QString &serviceName)
{
    // QProcess::execute("systemctl", QStringList() << "restart" << serviceName);
    QString cmd = QString("systemctl restart %1 > /dev/null 2>&1 & ").arg(serviceName);
    system(cmd.toUtf8().constData());

}

void AlsaRecConfigManager::restartService(const QString &serviceName)
{
    qDebug() <<  "AlsaRecConfigManager restartService:" << serviceName;
    // QProcess::execute("systemctl", QStringList() << "restart" << serviceName);
    QString cmd = QString("systemctl restart %1 > /dev/null 2>&1 & ").arg(serviceName);
    system(cmd.toUtf8().constData());
}

void AlsaRecConfigManager::stopService(const QString &serviceName)
{
    // QProcess::execute("systemctl", QStringList() << "stop" << serviceName);
    QString cmd = QString("systemctl stop %1 > /dev/null 2>&1 & ").arg(serviceName);
    system(cmd.toUtf8().constData());
}
void AlsaRecConfigManager::getAllConfigs(QWebSocket* sender)
{
    qDebug() << "AlsaRecConfigManager sender =" << sender;

    qDebug() << "[getAllConfigs] m_configs size =" << m_configs.size();
    qDebug() << "[getAllConfigs] keys =" << m_configs.keys();
    for (const QString &key : m_configs.keys()) {
        const AlsaRecConfig &config = m_configs[key];

        QJsonObject jsonObj;
        jsonObj["menuID"] = "AlsaRecConfigManager";
        jsonObj["RecID"] = config.recordID;
        jsonObj["recURI"] = QString("%1:%2").arg(config.rtsp_server_ip).arg(config.rtsp_server_port);
        jsonObj["iGateURI"] = QString("%1@%2").arg(config.rtsp_server_uri).arg(config.client_as_ip);
        jsonObj["iGateFreq"] = config.client_as_freq;
        jsonObj["alsa_dev"] = config.alsa_dev;
        jsonObj["enable"] = config.enable;

        QJsonDocument doc(jsonObj);
        QString jsonMessae = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
        qDebug() << "getAllConfigs_all_jsonMessage:" << jsonMessae;
        sender->sendTextMessage(jsonMessae);
    }
}
//void AlsaRecConfigManager::applyAllConfigs()
//{
//    qDebug() << "AlsaRecConfigManager_applyAllConfigs:";

//    for (const QString &key : m_configs.keys()) {
//        AlsaRecConfig &config = m_configs[key];

//        int recID = 0;
//        QRegularExpression re("REC_(\\d+)$");
//        QRegularExpressionMatch match = re.match(key);
//        if (match.hasMatch()) {
//            recID = match.captured(1).toInt();
//        }

//        config.recordID = recID;

//        if (config.enable) {
//            startService(config.service);
//        }
//    }

//    autoAnnounce();
//}



void AlsaRecConfigManager::checkServerAlive()
{
    qint64 currentDateTime = QDateTime::currentSecsSinceEpoch();
    for (const QString &key : m_configs.keys()) {
        AlsaRecConfig &config = m_configs[key];
        //        qDebug() << key << ":Service is In-Active.";
        if (config.enable) {
            if ((currentDateTime - config.lastMessage) > 10)
            // if (!isPortOpen(config.rtsp_server_ip, config.rtsp_server_port))
            {
                qDebug() << key << ":Service is In-Active.";
                RecorderSocketServer->sendSquelchStatus(config.recordID, false, false, false, "IDLE", config.client_as_freq);
                config.status = "IDLE";
                if ((currentDateTime - config.lastMessage) > 100){
                    config.lastMessage = currentDateTime;
                    restartService(config.service);
                }
            }
        }
    }
}


//void AlsaRecConfigManager::checkServerAlive()
//{
//    qint64 currentDateTime = QDateTime::currentSecsSinceEpoch();

//    for (const QString &key : m_configs.keys()) {
//        AlsaRecConfig &config = m_configs[key];

//        if (!config.enable)
//            continue;

//        if ((currentDateTime - config.lastMessage) > 10) {
//            qDebug() << key << ":Service is In-Active.";
//            qDebug() << "[checkServerAlive] RecorderSocketServer pointer =" << RecorderSocketServer;

//            if (RecorderSocketServer) {
//                RecorderSocketServer->sendSquelchStatus(
//                    config.recordID,
//                    false, false, false,
//                    "IDLE",
//                    config.client_as_freq
//                );
//            } else {
//                qWarning() << "[checkServerAlive] RecorderSocketServer is NULL -> skip sendSquelchStatus";
//            }

//            config.status = "IDLE";

//            if ((currentDateTime - config.lastMessage) > 100) {
//                config.lastMessage = currentDateTime;

//                if (!config.service.isEmpty()) {
//                    restartService(config.service);
//                } else {
//                    qWarning() << "[checkServerAlive] service empty for" << key;
//                }
//            }
//        }
//    }
//}


//void AlsaRecConfigManager::checkServerAlive()
//{
//    qint64 currentDateTime = QDateTime::currentSecsSinceEpoch();
//    for (const QString &key : m_configs.keys()) {
//        AlsaRecConfig &config = m_configs[key];
//        if (config.enable) {
//            if ((currentDateTime - config.lastMessage) > 10)
//            // if (!isPortOpen(config.rtsp_server_ip, config.rtsp_server_port))
//            {
//                qDebug() << key << ":Service is In-Active.";
//                RecorderSocketServer->sendSquelchStatus(config.recordID, false, false, false, "IDLE", config.client_as_freq);
//                config.status = "IDLE";
//                if ((currentDateTime - config.lastMessage) > 100){
//                    config.lastMessage = currentDateTime;
//                    restartService(config.service);
//                }
//            }
//        }
//    }
//}

// void AlsaRecConfigManager::applyAllConfigs()
// {
//     QList<AlsaRecConfig> activeConfigs;
//     for (const QString &key : m_configs.keys()) {
//         const AlsaRecConfig &config = m_configs[key];
//         if (config.enable) {
//             activeConfigs.append(config);
//         }
//     }

//     if (activeConfigs.isEmpty()) {
//         qDebug() << "No active configs to start.";
//         return;
//     }

//     // ใช้ index ไล่ทีละตัว
//     int intervalMs = 5*1000;
//     int currentIndex = 0;

//     QTimer *timer = new QTimer(this);
//     connect(timer, &QTimer::timeout, this, [=]() mutable {
//         if (currentIndex >= activeConfigs.size()) {
//             timer->stop();
//             timer->deleteLater();
//             return;
//         }

//         const AlsaRecConfig &cfg = activeConfigs.at(currentIndex);
//         qDebug() << cfg.service << ": Starting service...";
//         startService(cfg.service);

//         currentIndex++;
//     });

//     timer->start(intervalMs);

//     // เรียก startService ตัวแรกทันทีเลย (หรือรอ 5 วินาทีเหมือนกันแล้วแต่ต้องการ)
//     // ถ้าอยาก delay ตัวแรกด้วย ให้ลบบรรทัดนี้
//     const AlsaRecConfig &cfg = activeConfigs.at(0);
//     qDebug() << cfg.service << ": Starting service immediately...";
//     startService(cfg.service);
//     currentIndex = 1;
//     loopAnnounceTimer->start(10*1000);
// }
