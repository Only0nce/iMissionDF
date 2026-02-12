#include "Mainwindows.h"
#include "logwatcher.h"
#include "qthread.h"
#include "InputEventReader.h"
#include <QProcess>

Mainwindows::Mainwindows(QObject *parent) : QObject(parent)
{
    system("systemctl stop alsarecd.service");
    // iPatchServerSocket = new SocketClient;
    socketClientReconnectTimer = new QTimer(this);
    // wsClient = new WebSocketClient;
    myDatabase = new Database("ScanRF","orinnx","Ifz8zean6868**","127.0.0.1");
    qDebug() << "MYDATABASE ONLYONE";
    // wsClient.connectToServer(QUrl("ws://192.168.10.58:8073/ws/"));
    wsClient.connectToServer(QUrl("ws://127.0.0.1:8073/ws/"));
    // สร้าง worker + ย้ายไป thread
    m_setFreqWorker = new SetFreqWorker();
    m_setFreqWorker->moveToThread(&m_setFreqThread);

    // จบ thread แล้วลบ worker
    connect(&m_setFreqThread, &QThread::finished, m_setFreqWorker, &QObject::deleteLater);

    // รับผลกลับ
    connect(m_setFreqWorker, &SetFreqWorker::setFreqDone,
            this, &Mainwindows::onSetFreqDone, Qt::QueuedConnection);

    m_setFreqThread.start();

    // ส่งค่า host/port เริ่มต้นไปให้ worker
    QMetaObject::invokeMethod(m_setFreqWorker, "setHostPort",
                              Qt::QueuedConnection,
                              Q_ARG(QString, nc_host),
                              Q_ARG(quint16, nc_port));

    // rfdc->setHost("192.168.10.5");
    // rfdc->setPort(6000);
    // rfdc->setAutoReconnect(true);
    // rfdc->setReconnectIntervalMs(10000);
    // rfdc->connectToServer();
    // wsClient.connectToServer(QUrl("ws://192.168.10.26:8073/ws/"));
    InputEventReader *rotary = new InputEventReader("/dev/input/by-path/platform-rotary@1-event");
    InputEventReader *buttons = new InputEventReader("/dev/input/by-path/platform-gpio-keys-event");
    // connect(iPatchServerSocket,&SocketClient::newCommandProcess,this,&Mainwindows::newCommandProcess);
    connect(socketClientReconnectTimer,&QTimer::timeout,this,&Mainwindows::socketClientReconnect);
    connect(&wsClient,&WebSocketClient::spectrumUpdated,this,&Mainwindows::spectrumUpdated);
    connect(&wsClient,&WebSocketClient::waterfallUpdated,this,&Mainwindows::waterfallUpdated);
    connect(&wsClient,&WebSocketClient::smeterValueUpdated,this,&Mainwindows::smeterValueUpdated);
    connect(&wsClient,&WebSocketClient::waterfallColorMap,this,&Mainwindows::waterfallColorUpdate);
    connect(&wsClient,&WebSocketClient::waterfallLevelsChanged,this,&Mainwindows::waterfallLevelsChanged);
    connect(&wsClient,&WebSocketClient::updateCenterFreq,this,&Mainwindows::onCenterFreqChanged);
    connect(&wsClient,&WebSocketClient::openwebrxConnected,this,&Mainwindows::openwebrxConnected);
    connect(&wsClient,&WebSocketClient::updateProfiles,this,&Mainwindows::updateProfiles);
    connect(&wsClient,&WebSocketClient::onSQLChanged,this,&Mainwindows::onSQLChanged);
    connect(&wsClient,&WebSocketClient::onTemperatureChanged,this,&Mainwindows::onTemperatureChanged);
    connect(fileUpdateWatcher,&FileUpdateWatcher::fileAppearedOrChanged,this,&Mainwindows::fileUpdated);

    connect(wsServer,&ChatServer::onNewClientConneced,this,&Mainwindows::onNewClientConneced);
    connect(wsServer,&ChatServer::newSettingPageConnectd,this,&Mainwindows::newSettingPageConnectd);
    connect(wsServer,&ChatServer::newCommandProcess,this,&Mainwindows::newCommandProcess);

    connect(webServer,&ChatServer::onNewClientConneced,this,&Mainwindows::onNewClientConneceds);
    // connect(webServer,&ChatServer::newSettingPageConnectd,this,&Mainwindows::newSettingPageConnectd);
    connect(webServer,&ChatServer::newCommandProcess,this,&Mainwindows::newCommandProcessWeb);

    //Database
    connect(this,&Mainwindows::insertScanCard,myDatabase,&Database::insertScanCard);
    connect(this,&Mainwindows::deleteScanCardById,myDatabase,&Database::deleteScanCardById);
    connect(this,&Mainwindows::deleteScanCardAll,myDatabase,&Database::deleteScanCardAll);
    connect(this,&Mainwindows::deleteScanCardGroup,myDatabase,&Database::deleteScanCardGroup);
    // connect(myDatabase,&Database::initValue,this,&Mainwindows::initValue);
    connect(myDatabase,&Database::initValueJson,this,&Mainwindows::initValueJson);
    // connect(this,&Mainwindows::findBandsWithProfile,this,&Mainwindows::slotFindBandsWithProfile);
    //=============================================================================================
    AlsaRecConfigManager *manager = new AlsaRecConfigManager;

    connect(this,&Mainwindows::onSendSquelchStatus,manager,&AlsaRecConfigManager::sendSquelchStatus);
    connect(wsServer,&ChatServer::onSendSquelchStatus,manager,&AlsaRecConfigManager::sendSquelchStatus);
    connect(manager,&AlsaRecConfigManager::sendMessageToWeb,wsServer,&ChatServer::sendToWebMessageClient);
    connect(wsServer,&ChatServer::handleApplyRecSettings,manager,&AlsaRecConfigManager::handleApplyRecSettings);
    //    connect(wsServer, &ChatServer::getSystemPage,this, &Mainwindows::getSystemPage);
    connect(wsServer, &ChatServer::getVuMeter,manager, &AlsaRecConfigManager::getAllConfigs);
    //    connect(wsServer, &ChatServer::getServerHomePage,this, &Mainwindows::getServerHomePage);
    //    connect(this, &Mainwindows::SquelchStatusChange,wsServer, &ChatServer::SquelchStatusChange);

    QTimer::singleShot(1000, [manager]() {
        manager->loadConfig(ALSARECCONF);
        manager->applyAllConfigs();
    });
    //=============================================================================================

    scanrf = new ScanRF;
    myDatabase->getFrequency();
    socketClientReconnectTimer->start(3000);
    system("sh /usr/local/bin/noblank.sh");
    system("pulseaudio --kill ");

    gpioInit();
    codecDSPinit();
    openWebRxConfig.loadFromFile("/var/lib/openwebrx/settings.json");
    QJsonObject updateMsg = openWebRxConfig.generateProfileListMessage();
    QJsonDocument doc(updateMsg);
    qDebug().noquote() << "openWebRxConfig" << doc.toJson(QJsonDocument::Compact);

    QObject::connect(rotary, &InputEventReader::rotaryTurned,this,&Mainwindows::updateRotaryProfilesSlot);


    QObject::connect(buttons, &InputEventReader::keyPressed,this,&Mainwindows::updateGPIOKeyProfilesSlot);

    QObject::connect(buttons, &InputEventReader::keyReleased, [](int code) {
        if (code == 29){

        }
    });

    int ret = pthread_create(&idThreadSqlWatcher, nullptr, ThreadFuncSqlWatcher, this);
    if(ret==0){
        qDebug() <<("Thread created successfully.\n");
    }
    // main thread (Mainwindows ctor/init)
    m_lastRecIsRecord = false;
    m_lastRecState = "UNKNOWN";

//    LogWatcher *watcher = new LogWatcher(this);
//    connect(watcher, &LogWatcher::stateChanged, this, [this](const QString &id, const QString &conn, const QString &state){
//         qDebug() << "State update: ID=" << id << "conn=" << conn << "state=" << state;
//        this->recRunningCount = 0;
//        emit onRecStatusChanged(state == "RECORD");
//        if ((state == "RECORD") && (currentSQLValue == false))
//        {
//            onSQLChanged(currentSQLValue);
//        }
//        else if ((state == "PAUSE") && (currentSQLValue == true))
//        {
//            onSQLChanged(currentSQLValue);
//        }
//    });
//    watcher->startWatching("/tmp/alsarecd_production.log");
    LogWatcher *watcher = new LogWatcher(this);
    connect(watcher, &LogWatcher::stateChanged, this,
            [this](const QString &id, const QString &conn, const QString &state)
    {
        Q_UNUSED(id)
        Q_UNUSED(conn)

        m_lastRecState = state;
        m_lastRecIsRecord = (state == "RECORD");
        recRunningCount = 0;

        emit onRecStatusChanged(m_lastRecIsRecord);

        if ((state == "RECORD") && (currentSQLValue == false)) onSQLChanged(currentSQLValue);
        else if ((state == "PAUSE") && (currentSQLValue == true)) onSQLChanged(currentSQLValue);
    });

    watcher->startWatching("/tmp/alsarecd_id_1.log");


    startScanCard = new QTimer();
    QTimer *recRunningCountTimer = new QTimer(this);  // Set 'this' as parent to manage memory
    connect(recRunningCountTimer, &QTimer::timeout, this, [this]() {
        this->recRunningCount += 1;
        if (this->recRunningCount == 5){
            emit onRecStatusChanged(false);
            this->recRunningCount = 0;
        }
    });

    connect(startScanCard, SIGNAL(timeout()), this, SLOT(startScanCardFn()));
    startScanCard->setSingleShot(true);

    recRunningCountTimer->start(1000);  // Increment every second
    system("systemctl start alsarecd.service");

    squelchOffTimer = new QTimer(this);
    squelchOffTimer->setSingleShot(true);

    connect(squelchOffTimer, &QTimer::timeout, this, [this]() {
        if (isSquelchOffPending) {
            sendSquelchStatus(false); // call actual send function
            isSquelchOffPending = false;
        }
    });

    myDatabase->getAllScanCards();
    setHostPortNc();

    // QString outPath = "/var/lib/openwebrx/scanpreset.json";
    // QFile outFile(outPath);

    // if (outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    //     QJsonObject root;
    //     root["profiles"] = QJsonArray();  // ว่างเปล่า []

    //     QJsonDocument outDoc(root);
    //     outFile.write(outDoc.toJson(QJsonDocument::Indented));
    //     outFile.close();

    //     qDebug() << "[profileWeb] Wrote EMPTY JSON to:" << outPath;
    // }
    // else {
    //     qDebug() << "[profileWeb] Cannot open for write:" << outPath;
    // }

}

static bool splitIpCidrV4(const QString &cidr, QString &ip, QString &netmask, int &prefix)
{
    ip.clear();
    netmask.clear();
    prefix = -1;

    const QStringList parts = cidr.split('/');
    if (parts.size() != 2)
        return false;

    const QString ipPart = parts[0].trimmed();
    bool ok = false;
    const int p = parts[1].trimmed().toInt(&ok);
    if (!ok || p < 0 || p > 32)
        return false;

    QHostAddress ipAddr;
    if (!ipAddr.setAddress(ipPart))
        return false;

    // ensure IPv4
    if (ipAddr.protocol() != QAbstractSocket::IPv4Protocol)
        return false;

    quint32 mask = (p == 0) ? 0u : (0xFFFFFFFFu << (32 - p));

    ip = ipPart;
    netmask = QHostAddress(mask).toString();
    prefix = p;
    return true;
}

void Mainwindows::setHostPortNc()
{
    QVariantMap result = netWorkController->loadAllLanConfig();

    QJsonObject jsonObj = QJsonObject::fromVariantMap(result);

    QVariantMap networkMap = jsonObj.toVariantMap();
    QVariantMap lan   = networkMap["lan"].toMap();

    // QVariantMap lan1 = lan["lan1"].toMap();

    // QString lan1_iface = lan1["interface"].toString();
    // QString lan1_ip    = lan1["ip"].toString();
    // QString lan1_mode  = lan1["mode"].toString();
    // QString lan1_gw    = lan1["gateway"].toString();
    // QStringList lan1_dns = lan1["dns"].toStringList();

    // QVariantMap lan2 = lan["lan2"].toMap();

    // QString lan2_iface = lan2["interface"].toString();
    // QString lan2_ip    = lan2["ip"].toString();
    // QString lan2_mode  = lan2["mode"].toString();
    // QString lan2_gw    = lan2["gateway"].toString();
    // QStringList lan2_dns = lan2["dns"].toStringList();

    // QVariantMap rfsoc1 = lan["rfsoc1"].toMap();

    // QString rfsoc1_iface = rfsoc1["interface"].toString();
    // QString rfsoc1_ip    = rfsoc1["ip"].toString();
    // QString rfsoc1_mode  = rfsoc1["mode"].toString();
    // QString rfsoc1_gw    = rfsoc1["gateway"].toString();
    // QStringList rfsoc1_dns = rfsoc1["dns"].toStringList();

    // QVariantMap rfsoc2 = lan["rfsoc2"].toMap();

    // QString rfsoc2_iface = rfsoc2["interface"].toString();
    // QString rfsoc2_ip    = rfsoc2["ip"].toString();
    // QString rfsoc2_mode  = rfsoc2["mode"].toString();
    // QString rfsoc2_gw    = rfsoc2["gateway"].toString();
    // QStringList rfsoc2_dns = rfsoc2["dns"].toStringList();

    auto dumpLan = [&](const QString &key) {
        QVariantMap m = lan.value(key).toMap();

        const QString iface = m.value("interface").toString();
        const QString cidr  = m.value("ip").toString();      // "192.168.10.31/24"
        const QString mode  = m.value("mode").toString();
        const QString gw    = m.value("gateway").toString();
        const QStringList dns = m.value("dns").toStringList();

        QString ip, netmask;
        int prefix = -1;
        const bool ok = splitIpCidrV4(cidr, ip, netmask, prefix);

        if(iface == "end0"){
            setHostPortNc(ip,6000);
        }

        qDebug().noquote()
            << "\n[" << key << "]"
            << "\n  iface   =" << iface
            << "\n  mode    =" << mode
            << "\n  cidr    =" << cidr
            << "\n  ip      =" << (ok ? ip : QString("<invalid>"))
            << "\n  netmask =" << (ok ? netmask : QString("<invalid>"))
            << "\n  prefix  =" << (ok ? QString::number(prefix) : QString("<invalid>"))
            << "\n  gw      =" << gw
            << "\n  dns     =" << dns;

        // ถ้าคุณต้องการเอาไปใช้ต่อจริง ๆ (เช่น apply network / setHostPort)
        // ก็ return ออกไปเป็น struct ได้ หรือเก็บใส่ member map
    };

    dumpLan("lan1");
    dumpLan("lan2");
    dumpLan("rfsoc1");
    dumpLan("rfsoc2");
    // QJsonDocument jsonDoc(jsonObj);
    // QString jsonString = QString::fromUtf8(jsonDoc.toJson(QJsonDocument::Compact));
}


void Mainwindows::setHostPortNc(const QString& host, quint16 port)
{
    nc_host = host;
    nc_port = port;

    if (m_setFreqWorker) {
        QMetaObject::invokeMethod(m_setFreqWorker, "setHostPort",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, nc_host),
                                  Q_ARG(quint16, nc_port));
    }
}

void Mainwindows::requestSetFreqAsync(quint64 freqHz, int timeoutMs)
{
    if (!m_setFreqWorker) return;

    qWarning() << "[UI] requestSetFreqAsync freqHz=" << freqHz;

    QMetaObject::invokeMethod(m_setFreqWorker, "requestSetFreq",
                              Qt::QueuedConnection,
                              Q_ARG(quint64, freqHz),
                              Q_ARG(int, timeoutMs));
}

void Mainwindows::onSetFreqDone(quint64 freqHz, bool ok)
{
    qWarning() << "[UI] setFreq done freqHz=" << freqHz << "ok=" << ok;

    // ถ้าจะ update UI ทำที่นี่ได้เลย ปลอดภัย (กลับมา UI thread แล้ว)
}

void Mainwindows::onRecorderConfigSaved()
{
    QVariantMap configMap = recConfig->loadConfig();
    // Convert to QJsonObject
    QJsonObject jsonObj = QJsonObject::fromVariantMap(configMap);

    // Convert to JSON string
    QJsonDocument doc(jsonObj);
    QString jsonString = doc.toJson(QJsonDocument::Compact);
    wsServer->broadcastMessage(jsonString);
}
void Mainwindows::newSettingPageConnectd(QWebSocket *pSender)
{
    // QVariantMap result = netWorkController->loadConfig("bond0");
    QVariantMap result = netWorkController->loadAllLanConfig();

    QJsonObject jsonObj = QJsonObject::fromVariantMap(result);
    QJsonDocument jsonDoc(jsonObj);
    QString jsonString = QString::fromUtf8(jsonDoc.toJson(QJsonDocument::Compact));

    pSender->sendTextMessage(jsonString);
}
void Mainwindows::onNewClientConneced(QWebSocket *socketClient)
{
    QVariantMap configMap = recConfig->loadConfig();
    // Convert to QJsonObject
    QJsonObject jsonObj = QJsonObject::fromVariantMap(configMap);

    // Convert to JSON string
    QJsonDocument doc(jsonObj);
    QString jsonString = doc.toJson(QJsonDocument::Compact);
    socketClient->sendTextMessage(jsonString);
    updateCurrentOffsetFreq(currentOffsetFreq,currentCenterFreq);
    QJsonObject cfg = netWorkController->getNtpConfig();
    QJsonObject msg;
    msg["menuID"] = "getNtpConfig";
    msg["payload"] = cfg;

    wsServer->broadcastMessage(
        QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact))
        );

    QString tzObj = netWorkController->getTimezone();

    timeLocation = tzObj;

    QJsonObject msgs;
    msgs["menuID"] = "getTimezone";
    msgs["timezone"] = tzObj;

    wsServer->broadcastMessage(
        QString::fromUtf8(
            QJsonDocument(msgs).toJson(QJsonDocument::Compact)
            )
        );
}

void Mainwindows::onNewClientConneceds(QWebSocket *socketClient)
{
    qDebug() << "socketClient:" << socketClient;
    if (webServer) {
        QJsonObject sendObj;
        sendObj["objectName"] = "profilesCard";
        sendObj["profiles"]   = cardsProfilesArray;

        QJsonDocument sendDoc(sendObj);
        QString raw_data = QString::fromUtf8(sendDoc.toJson(QJsonDocument::Compact));

        webServer->broadcastMessage(raw_data);
        // cardsProfilesArray = QJsonArray();
        qDebug() << "[profileWeb] Broadcast profilesCard to web, bytes:" << raw_data.size();
    } else {
        qDebug() << "[profileWeb] webServer is null, cannot broadcast profiles";
    }
}

void Mainwindows::fileUpdated(const QString &path)
{
    qDebug() << "fileUpdated found" << path;

    if (path.contains("update.tar")) {
        QStringList args;
        args << "-p" << "/tmp/update/";  // Optional: extract to /
        int result = QProcess::execute("mkdir", args);
        qDebug() << "update:" << result;
    }
    QProcess::execute("sync");

    if (path.contains("update.tar")) {
        QStringList args;
        args << "-xf" << path << "-C" << "/tmp/update/";  // Optional: extract to /
        int result = QProcess::execute("tar", args);
        qDebug() << "Tar extract result:" << result;
    }
    QProcess::execute("sync");

    if (path.contains("update.tar")) {
        QStringList args;
        args << path;
        int result = QProcess::execute("rm", args);
        qDebug() << "remove:" << result;
    }

    if (path.contains("update.tar")) {
        QStringList args;
        args << "/tmp/update/update.sh";
        int result = QProcess::execute("sh", args);
        qDebug() << "update:" << result;
    }
    QProcess::execute("sync");
    exit(0);
}

Q_INVOKABLE void Mainwindows::shutdownRequested()
{
    backlightOff();

    //resetHW
    dspBootSelect->setValue(QSPIFLASH);
    codecReset->setValue(RESET_ACTIVE);
    dspReset->setValue(RESET_ACTIVE);
    led3->setValue(LED_OFF);
    led4->setValue(LED_OFF);
    QProcess::execute("sudo shutdown now");
}
Q_INVOKABLE void Mainwindows::rebootRequested()
{
    backlightOff();

    //resetHW
    dspBootSelect->setValue(QSPIFLASH);
    codecReset->setValue(RESET_ACTIVE);
    dspReset->setValue(RESET_ACTIVE);
    led3->setValue(LED_OFF);
    led4->setValue(LED_OFF);
    QProcess::execute("sudo reboot"); // or "shutdown now"
}
Q_INVOKABLE void Mainwindows::offScreenRequested()
{
    backlightOff();
    qDebug() << "Mainwindows::offScreenRequested()";
}
Q_INVOKABLE void Mainwindows::backlightRequested()
{
    backlight->setValue(true);
    QThread::msleep(100);
    backlight->setValue(false);
    QThread::msleep(100);
    backlight->setValue(true);
    qDebug() << "Mainwindows::backlightRequested()";
}
Q_INVOKABLE void Mainwindows::onScreenRequested()
{
    backlightOn();
    qDebug() << "Mainwindows::onScreenRequested()";
}

Q_INVOKABLE void Mainwindows::refreshProfiles() {
    QJsonObject updateMsg = openWebRxConfig.generateProfileListMessage();
    qDebug() << "refreshProfiles" << updateMsg;
    emit updateProfiles(updateMsg["value"].toArray());
}

void Mainwindows::updateCurrentOffsetFreq(const int value, const double centerFreq)
{
    currentCenterFreq = centerFreq;
    currentOffsetFreq = value;
    onSQLChanged(currentSQLValue);
}

void Mainwindows::onSQLChanged(bool sqlVal)
{
    qDebug() << "onSQLChanged" << sqlVal;
    if ((currentSQLValue != sqlVal) && (sqlVal == false)){
        squelchOffTimer->start(100);  // 5 seconds
        qDebug() << "Scheduled squelch OFF in 100 msec";
    }

    bool current;
    if (shd_amp->getValue(current) == 0) {

    }

    currentSQLValue = sqlVal;
    if (sqlVal)
    {
        if (current != 1) {
            shd_amp->setValue(1);
            hs_mute->setValue(1);
        }
        led4->setValue(LED_ON);
        // cancel pending OFF
        if (squelchOffTimer->isActive()) {
            squelchOffTimer->stop();
            isSquelchOffPending = false;
            qDebug() << "Cancelled pending squelch OFF due to ON";
        }


        sendSquelchStatus(true);  // send immediately
    } else {
        if (current != 0) {
            shd_amp->setValue(0);
            hs_mute->setValue(0);
        }
        led4->setValue(LED_OFF);
        // schedule delayed squelch OFF
        isSquelchOffPending = true;
        if (squelchOffTimer->isActive() == false){
            squelchOffTimer->start(100);  // 5 seconds
            qDebug() << "Scheduled squelch OFF in 100 msec";
        }
    }
}
//void Mainwindows::sendSquelchStatus(bool sqlVal)
//{
//    QJsonObject message;
//    message["object"] = "receiverStatus";
//    message["squelch"] = sqlVal ? "on" : "off";
//    message["device"] = "recin1";
//    message["recorder"] = this->recEnable ? "enable" : "disable";
//    message["frequency"] = this->currentCenterFreq + this->currentOffsetFreq;

//    QJsonDocument doc(message);
//    QString jsonString = doc.toJson(QJsonDocument::Compact);
//    qDebug() << "broadcastMessage_sendSquelchStatus:" << jsonString << sqlVal;

//    int softPhoneID = 1;

//    bool pttOn = false;                 // ถ้ายังไม่มี PTT ก็ fix ไว้ก่อน
//    bool sqlOn = sqlVal;                // ✅ ใช้ตัวจริง
//    bool callState = this->recEnable;   // ✅ ใช้ตัวจริง
//    qint64 freqHz = qRound64(this->currentCenterFreq + this->currentOffsetFreq);
//    double freqMHz = (freqHz > 0) ? (double)freqHz / 1e6 : 0.0;
////    qint64 freqHz = static_cast<qint64>(qRound64(this->currentCenterFreq + this->currentOffsetFreq));
//    qDebug() << "onSendSquelchStatus_to_Alsarecd:" << softPhoneID
//             << pttOn << sqlOn << callState << freqHz << freqMHz;
//    emit onSendSquelchStatus(softPhoneID, pttOn, sqlOn, callState, freqMHz);
//    emit frequencyChangedToQml(freqHz, freqMHz);

////    emit onSendSquelchStatus(softPhoneID, pttOn, sqlOn, callState, (double)freqHz);
//}


void Mainwindows::sendSquelchStatus(bool sqlVal)
{
    QJsonObject message;
    message["object"] = "receiverStatus";
    message["squelch"] = sqlVal ? "on" : "off";
    message["device"] = "recin1";
    message["recorder"] = this->recEnable ? "enable" : "disable";
    message["frequency"] = this->currentCenterFreq + this->currentOffsetFreq;

    QJsonDocument doc(message);
    QString jsonString = doc.toJson(QJsonDocument::Compact);
    qDebug() << "broadcastMessage_sendSquelchStatus:" << jsonString << sqlVal;

    int softPhoneID = 1;

    bool pttOn = false;                 // ถ้ายังไม่มี PTT ก็ fix ไว้ก่อน
    bool sqlOn = sqlVal;                // ✅ ใช้ตัวจริง
    bool callState = this->recEnable;   // ✅ ใช้ตัวจริง
    qint64 freqHz = qRound64(this->currentCenterFreq + this->currentOffsetFreq);
    double freqMHz = (freqHz > 0) ? (double)freqHz / 1e6 : 0.0;
//    qint64 freqHz = static_cast<qint64>(qRound64(this->currentCenterFreq + this->currentOffsetFreq));
    qDebug() << "onSendSquelchStatus_to_Alsarecd:" << softPhoneID
             << pttOn << sqlOn << callState << freqHz << freqMHz;
    emit onSendSquelchStatus(softPhoneID, pttOn, sqlOn, callState, freqMHz);
    emit frequencyChangedToQml(freqHz, freqMHz);
//    emit onSendSquelchStatus(softPhoneID, pttOn, sqlOn, callState, (double)freqHz);
}


//void Mainwindows::sendSquelchStatus(bool sqlVal)
//{
//    QJsonObject message;
//    message["object"] = "receiverStatus";
//    message["squelch"] = sqlVal ? "on" : "off";
//    message["device"] = "recin1";
//    message["recorder"] = this->recEnable ? "enable" : "disable";
//    message["frequency"] = this->currentCenterFreq + this->currentOffsetFreq;

//    QJsonDocument doc(message);
//    QString jsonString = doc.toJson(QJsonDocument::Compact);

//    wsServer->broadcastMessage(jsonString);
//}


void Mainwindows::updateGPIOKeyProfilesSlot(int code)
{
    if (code == 30){
        emit updateGPIOKeyProfiles(code);
    }
}
void Mainwindows::updateRotaryProfilesSlot(int dir)
{
    // qDebug() << "Rotary turned:" << (dir == 1 ? "CCW" : "CW");
    emit updateRotaryProfiles(dir);
}
void Mainwindows::openwebrxConnected()
{
    // sendmessage('{"type": "dspcontrol","params": {"squelch_level": '+(scanSqlLevel-255/2).toFixed(1)+'}}')
    QJsonObject params;
    params["squelch_level"] = ((scanSqlLevel-255)/2);

    QJsonObject json;
    json["type"] = "dspcontrol";
    json["params"] = params;

    sendmessage(QJsonDocument(json).toJson(QJsonDocument::Compact));
    emit onOpenwebrxConnected();
}

void Mainwindows::codecDSPinit()
{
    system("speaker-test -Dhw:APE,0 -r8000 -c8 -S0 --nloops 3 -s 1 -tsine -f1000");
    SigmaFirmWareDownLoad = new ADAU1467("/dev/spidev0.0");

    SigmaFirmWareDownLoad->default_download_IC_1();
    QThread::msleep(200);
    SigmaFirmWareDownLoad->setToneVolume(TONE_CH1_ADDRESS,0);
    SigmaFirmWareDownLoad->setToneVolume(TONE_CH2_ADDRESS,0);
    SigmaFirmWareDownLoad->setToneVolume(TONE_CH3_ADDRESS,0);
    SigmaFirmWareDownLoad->setToneVolume(TONE_CH4_ADDRESS,0);

    SigmaFirmWareDownLoad->setToneVolume(TONE_SERVER_CH1_ADDRESS,0);
    SigmaFirmWareDownLoad->setToneVolume(TONE_SERVER_CH2_ADDRESS,0);
    SigmaFirmWareDownLoad->setToneVolume(TONE_SERVER_CH3_ADDRESS,0);
    SigmaFirmWareDownLoad->setToneVolume(TONE_SERVER_CH4_ADDRESS,0);

    CODEC_PCM3168A = new PCM3168A(CODEC1I2CDEV,CODECI2CADDR_PCM3168A);
    qDebug() << "CODEC_PCM3168A" << CODEC_PCM3168A->active;

    SigmaFirmWareDownLoad->setMixerVolume(AUDIOIN_VOLUME_CH1_ADDRESS,SIDETONE_VOLUME_CH1_MODE_ADDRESS,SIDETONE_VOLUME_CH1_MODE_VALUE,1);
    SigmaFirmWareDownLoad->setMixerVolume(AUDIOIN_VOLUME_CH2_ADDRESS,SIDETONE_VOLUME_CH2_MODE_ADDRESS,SIDETONE_VOLUME_CH2_MODE_VALUE,1);
    SigmaFirmWareDownLoad->setMixerVolume(AUDIOIN_VOLUME_CH3_ADDRESS,SIDETONE_VOLUME_CH3_MODE_ADDRESS,SIDETONE_VOLUME_CH3_MODE_VALUE,1);
    SigmaFirmWareDownLoad->setMixerVolume(AUDIOIN_VOLUME_CH4_ADDRESS,SIDETONE_VOLUME_CH4_MODE_ADDRESS,SIDETONE_VOLUME_CH4_MODE_VALUE,1);
    SigmaFirmWareDownLoad->setFIRfilter(MOD_FIR1_ALG0_FIRSIGMA300ALG1FIRCOEFF0_ADDR,MOD_FIR1_COUNT,FIRfilter_stateON_INPUT);
    SigmaFirmWareDownLoad->setFIRfilter(MOD_FIR2_ALG0_FIRSIGMA300ALG5FIRCOEFF0_ADDR, MOD_FIR2_COUNT,FIRfilter_stateON_OUTPUT);

    CODEC_PCM3168A->setInputGain(CODECCH1_I2S1,VolumeInCH1);
    CODEC_PCM3168A->setInputGain(CODECCH2_I2S1,VolumeInCH2);
    CODEC_PCM3168A->setInputGain(CODECCH3_I2S1,VolumeInCH3);
    CODEC_PCM3168A->setInputGain(CODECCH4_I2S1,VolumeInCH4);

    CODEC_PCM3168A->setOutputGain(CODECCH1_I2S1,CODECVolumeOutCH1);
    CODEC_PCM3168A->setOutputGain(CODECCH2_I2S1,CODECVolumeOutCH2);
    CODEC_PCM3168A->setOutputGain(CODECCH3_I2S1,CODECVolumeOutCH3);
    CODEC_PCM3168A->setOutputGain(CODECCH4_I2S1,CODECVolumeOutCH4);

    CODEC_PCM3168A->setOutputGain(5,0); //mute
    CODEC_PCM3168A->setOutputGain(6,0); //mute
    CODEC_PCM3168A->setOutputGain(7,0); //mute
    CODEC_PCM3168A->setOutputGain(8,0); //mute

    updateDSPOutputGain(DSPVolumeOutCH1,CODECCH1_I2S1);
    updateDSPOutputGain(DSPVolumeOutCH2,CODECCH2_I2S1);
    updateDSPOutputGain(DSPVolumeOutCH3,CODECCH3_I2S1);
    updateDSPOutputGain(DSPVolumeOutCH4,CODECCH4_I2S1);

    updateDSPSpeakerOutputGain(255-VolumeOutCH1,CODECCH1_I2S1);
    updateDSPSpeakerOutputGain(255-VolumeOutCH2,CODECCH2_I2S1);
    updateDSPSpeakerOutputGain(255-VolumeOutCH3,CODECCH3_I2S1);
    updateDSPSpeakerOutputGain(255-VolumeOutCH4,CODECCH4_I2S1);

    updateDSPRecInputGain(VolumeRecInDSPCH1,CODECCH1_I2S1);
    updateDSPRecInputGain(VolumeRecInDSPCH2,CODECCH2_I2S1);
    updateDSPRecInputGain(VolumeRecInDSPCH3,CODECCH3_I2S1);
    updateDSPRecInputGain(VolumeRecInDSPCH4,CODECCH4_I2S1);

    updateDSPRecOutputGain(VolumeRecOutDSPCH1,CODECCH1_I2S1);
    updateDSPRecOutputGain(VolumeRecOutDSPCH2,CODECCH2_I2S1);
    updateDSPRecOutputGain(VolumeRecOutDSPCH3,CODECCH3_I2S1);
    updateDSPRecOutputGain(VolumeRecOutDSPCH4,CODECCH4_I2S1);

    headphoneGpioOn->setValue(HEADPHONE_STANDBY);
    ampGpioMute->setValue(SPK_UNMUTE);
    ampGpioStandby->setValue(AMP_STANDBY);
}
Q_INVOKABLE void Mainwindows::setSqlLevel(const unsigned char value)
{
    scanSqlLevel = value;
}

Q_INVOKABLE void Mainwindows::setSqlOffManual()
{
    onSQLChanged(scanSqlLevel);
}

Q_INVOKABLE void Mainwindows::setSpeakerVolume(const unsigned char volume)
{
    qDebug() << "setSpeakerVolume" << volume;
    VolumeOutCH4 = volume;
    if(volume < 165){
        VolumeOutCH4 = 50;
    }
    // VolumeOutCH1 = volume;
    // VolumeOutCH2 = volume;
    // updateDSPOutputGain(255-VolumeOutCH1,CODECCH1_I2S1);
    // updateDSPOutputGain(255-VolumeOutCH2,CODECCH2_I2S1);
    // updateDSPSpeakerOutputGain(255-VolumeOutCH1,CODECCH1_I2S1);
    updateDSPSpeakerOutputGain(255-VolumeOutCH4,CODECCH4_I2S1);
}

// Q_INVOKABLE void Mainwindows::setSpeakerVolumeMute(bool active)
// {
//     if(active){
//         updateDSPSpeakerOutputGain(255-50,CODECCH4_I2S1);
//     }
//     else{
//         updateDSPSpeakerOutputGain(255-VolumeOutCH4,CODECCH4_I2S1);
//     }

// }

Q_INVOKABLE void Mainwindows::setHeadphoneVolume(const unsigned char volume)
{
    // VolumeOutCH3 = volume;
    // VolumeOutCH4 = volume;
    VolumeOutCH2 = volume;
    // updateDSPSpeakerOutputGain(255-VolumeOutCH3,CODECCH2_I2S1);
    // updateDSPSpeakerOutputGain(255-VolumeOutCH4,CODECCH4_I2S1);
    // updateDSPOutputGain(255-VolumeOutCH3,CODECCH3_I2S1);
    // updateDSPOutputGain(255-VolumeOutCH4,CODECCH4_I2S1);
    updateDSPSpeakerOutputGain(255-VolumeOutCH2,CODECCH2_I2S1);
}


Q_INVOKABLE void Mainwindows::sCanfreq(){
    qDebug() << "let's begin sCanfreq";

    QJsonObject params;
    QJsonObject msg;
    int i=-1600000;

    while(i<1600000){
        params["offset_freq"] = i;
        msg["type"]   = QStringLiteral("dspcontrol");
        msg["params"] = params;

        const QString json = QString::fromUtf8(QJsonDocument(msg).toJson(QJsonDocument::Compact));
        sendmessage(json);               // หรือ wsClient->sendTextMessage(json);
        qDebug() << "json" << json;
        QThread::msleep(500);
        i = i+1000;
    }

    qDebug() << "sCanfreq done";
}

void Mainwindows::gpioInit()
{
    codecReset->requestOutput();
    dspReset->requestOutput();
    dspBootSelect->requestOutput();
    led3->requestOutput();
    led4->requestOutput();
    headphoneGpioOn->requestOutput();
    ampGpioMute->requestOutput();
    ampGpioStandby->requestOutput();
    backlight->requestOutput();
    lna_1_enable->requestOutput();
    lna_2_enable->requestOutput();

    rotary_led->requestOutput();
    rst_amp->requestOutput();
    shd_amp->requestOutput();
    hs_mute->requestOutput();

    backlightOn();
    set_lna_1_enable();
    set_lna_2_disable(); // Distortion

    //resetHW
    dspBootSelect->setValue(QSPIFLASH);
    codecReset->setValue(RESET_ACTIVE);
    dspReset->setValue(RESET_ACTIVE);
    QThread::msleep(200);

    codecReset->setValue(RESET_INACTIVE);
    dspReset->setValue(RESET_INACTIVE);


    led3->setValue(LED_ON);
    led4->setValue(LED_ON);
    headphoneGpioOn->setValue(HEADPHONE_SHUTDOWN);
    ampGpioMute->setValue(SPK_MUTE);
    ampGpioStandby->setValue(AMP_SHUTDOWN);

    rotary_led->setValue(1);
    rst_amp->setValue(1);
    shd_amp->setValue(0);
    hs_mute->setValue(0);

    QThread::msleep(200);

}
void Mainwindows::DSPBootSelect(const bool qspiflash)
{
    dspBootSelect->setValue(qspiflash);
}

void Mainwindows::profiles(){
    qDebug() << "emit profiles::";
    emit updateCardProfile();
}

void Mainwindows::setNetworkFormDisplay(const QString &ipWithCidr){
    qDebug() << "setNetworkFormDisplay Kraken::" << ipWithCidr;
    // netWorkController->applyNetworkConfig("end0", "static", ipWithCidr, "", "");
    // QVariantMap result = netWorkController->loadAllLanConfig();
    // QJsonObject jsonObj = QJsonObject::fromVariantMap(result);
    // QJsonDocument jsonDoc(jsonObj);
    // QString jsonString = QString::fromUtf8(jsonDoc.toJson(QJsonDocument::Compact));

    // wsServer->broadcastMessage(jsonString);
}

void Mainwindows::setNetworkFormDisplay(const int index,
                                        const QString &mode,
                                        const QString &ipWithCidr,
                                        const QString &gateway,
                                        const QString &dnsList)
{
    // =========================================================
    // ✅ Only DNS: blank -> "0"
    // =========================================================
    const bool dnsWasBlank = dnsList.trimmed().isEmpty();
    const QString dnsNorm  = dnsWasBlank ? QStringLiteral("0")
                                        : dnsList.trimmed();

    qDebug() << "setNetworkFormDisplay All::"
             << "index=" << index
             << "mode=" << mode
             << "ip=" << ipWithCidr
             << "gateway=" << gateway
             << "dns(in)=" << dnsList
             << "dns(norm)=" << dnsNorm;

    // =========================================================
    // ✅ Resolve interface
    // =========================================================
    QString iface;
    if      (index == 0) iface = "enP8p1s0";
    else if (index == 1) iface = "enP1p1s0";
    else if (index == 2) iface = "end0";
    else if (index == 3) iface = "end1";
    else {
        qWarning() << "Invalid network index:" << index;
        return;
    }

    // =========================================================
    // ✅ Apply network config
    //    (applyNetworkConfig รองรับ dns = "0" แล้ว)
    // =========================================================
    netWorkController->applyNetworkConfig(
        iface,
        mode,
        ipWithCidr,
        gateway,
        dnsNorm          // ✅ ส่ง "0" เฉพาะกรณี dns ว่าง
        );

    // =========================================================
    // ✅ Load + broadcast full LAN config
    // =========================================================
    QVariantMap result = netWorkController->loadAllLanConfig();
    QJsonObject jsonObj = QJsonObject::fromVariantMap(result);
    QJsonDocument jsonDoc(jsonObj);
    QString jsonString =
        QString::fromUtf8(jsonDoc.toJson(QJsonDocument::Compact));

    // =========================================================
    // ✅ Command payload (ไม่ยุ่ง dns)
    // =========================================================
    QJsonObject params;
    params.insert("objectName", "Networks");
    params.insert("ipaddress", ipWithCidr);

    const QString raw_data =
        QJsonDocument(params).toJson(QJsonDocument::Compact);

    // =========================================================
    // ✅ Send to REC only for index == 1
    // =========================================================
    if (index == 1) {
        emit commandMainCppToRecCpp(raw_data);
    }

    // =========================================================
    // ✅ Broadcast to WebSocket clients
    // =========================================================
    wsServer->broadcastMessage(jsonString);
}


void Mainwindows::newCommandProcess(const QJsonObject command, QWebSocket *pSender, const QString &message)
{
    QString getCommand =  QJsonValue(command["menuID"]).toString();
    QString menuID     = command.value("menuID").toString().trimmed();
    // qDebug() << "Mainwindows::newCommandProcess getCommand:" << getCommand;
    if ((getCommand == "alarmResult") || (getCommand == "smartEvent"))
    {
        emit cppCommand(message);
    }
    else if (menuID == "updateNTPServer") {
        const QString ntpServer = command.value("ntpServer").toString().trimmed();
        qDebug() << "[WEB] updateNTPServer:" << ntpServer;

        // ตัวอย่าง: "192.168.10.1 time.google.com 1.pool.ntp.org"
        // เรียก NetworkController ที่คุณ refactor ไว้
        netWorkController->setNtpServer(ntpServer.isEmpty() ? "0.0.0.0" : ntpServer);
        return;
    }
    else if(menuID == "setLocation"){
        const QString location = command.value("location").toString().trimmed();
        if (location.isEmpty()) {
            qWarning() << "[WEB] setLocation: empty location";
            return;
        }
        setLocation(location);
    }
    else if (menuID == "updateTime") {
        const QString dtStr = command.value("dateTime").toString().trimmed();
        qDebug() << "[WEB] updateTime:" << dtStr;

        // รองรับหลาย format (ของ input / ISO)
        QDateTime dt;

        // 1) ISO (2026-01-12T10:20:00 / 2026-01-12T10:20)
        dt = QDateTime::fromString(dtStr, Qt::ISODate);
        if (!dt.isValid())
            dt = QDateTime::fromString(dtStr, "yyyy-MM-ddTHH:mm");
        if (!dt.isValid())
            dt = QDateTime::fromString(dtStr, "yyyy-MM-ddTHH:mm:ss");

        // 2) แบบไทย/ฟอร์มทั่วไป (2026-01-12 10:20:00)
        if (!dt.isValid())
            dt = QDateTime::fromString(dtStr, "yyyy-MM-dd HH:mm");
        if (!dt.isValid())
            dt = QDateTime::fromString(dtStr, "yyyy-MM-dd HH:mm:ss");

        if (!dt.isValid()) {
            qWarning() << "[WEB] updateTime invalid format:" << dtStr;
            return;
        }

        // ตั้งเวลา (ต้องสิทธิ์ root)
        // แนะนำ timedatectl เพราะชัดเจน
        const QString cmd = QString("timedatectl set-time '%1'")
                                .arg(dt.toString("yyyy-MM-dd HH:mm:ss"));

        QProcess p;
        p.start("/bin/bash", { "-c", cmd });
        p.waitForFinished();

        if (p.exitCode() != 0) {
            qWarning() << "[WEB] timedatectl failed:" << p.readAllStandardError();
        } else {
            qDebug() << "[WEB] time set ok";
        }
        return;
    }
    else if (getCommand == "heartbeat")
    {
        //        emit cppCommand(message);
    }
    else if (getCommand == "getSystemPage")
    {
        QJsonObject systemInfo;
        systemInfo["menuID"] = "system";
        systemInfo["SwVersion"] = "1.0";

        QJsonDocument doc(systemInfo);
        QString jsonString = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

        pSender->sendTextMessage(jsonString);
    }

    else if (getCommand == "applyNetwork")
    {

        QString iface = command.value("iface").toString();
        QString mode = command.value("mode").toString(); // "static" or "dhcp"
        QString ipWithCidr = command.value("ip").toString(); // e.g., "192.168.10.25/24"
        QString gateway = command.value("gateway").toString();
        QString dnsList = command.value("dns").toString();   // e.g., "192.168.10.254,8.8.8.8"

        // if(iface == "end0" || iface == "end1")
        //     qDebug() << "iface:" << getCommand << " command:" << command;
        // else
        netWorkController->applyNetworkConfig(iface, mode, ipWithCidr, gateway, dnsList);
        emit updateNetworkToDisplay(QString::fromUtf8(QJsonDocument(command).toJson(QJsonDocument::Compact)));

        QJsonDocument jsonDocs;
        QJsonObject Params;
        Params.insert("objectName", "Networks");
        Params.insert("ipaddress", ipWithCidr);
        jsonDocs.setObject(Params);
        QString raw_data = QJsonDocument(Params).toJson(QJsonDocument::Compact).toStdString().c_str();

        if(iface == "enP1p1s0"){
            emit commandMainCppToRecCpp(raw_data);
        }

        QVariantMap result = netWorkController->loadAllLanConfig();
        QJsonObject jsonObj = QJsonObject::fromVariantMap(result);
        QJsonDocument jsonDoc(jsonObj);
        QString jsonString = QString::fromUtf8(jsonDoc.toJson(QJsonDocument::Compact));

        wsServer->broadcastMessage(jsonString);
    }
    else if (getCommand == "rebootSystem")
    {
        backlightOff();

        //resetHW
        dspBootSelect->setValue(QSPIFLASH);
        codecReset->setValue(RESET_ACTIVE);
        dspReset->setValue(RESET_ACTIVE);
        led3->setValue(LED_OFF);
        led4->setValue(LED_OFF);
        QProcess::execute("sudo reboot"); // or "shutdown now"
    }
    else {
        // qDebug() << "newCommandProcess" << message;
    }
}

void Mainwindows::socketClientReconnect()
{
    // if(iPatchServerSocket->isConnected == false)
    // {
    //     iPatchServerSocket->createConnection(99,0,"127.0.0.1",1234);
    // }
}

void Mainwindows::sendmessageToWeb(const QString &jsonMessage)
{
    qDebug() << "sendmessageToWeb JSON to backend:" << jsonMessage;
    webServer->broadcastMessage(jsonMessage);
}

void Mainwindows::sendmessage(const QString &jsonMessage)
{
    qDebug() << "Send JSON to backend:" << jsonMessage;
    wsClient.webSocket.sendTextMessage(jsonMessage);
}
// void Mainwindows::sendMessage(const QString &jsonMsg)
// {
//     qDebug() << "sendMessage" << jsonMsg;
//     wsClient.webSocket.sendTextMessage(jsonMsg);
// }

void Mainwindows::sCan(const QString&  mode){
    qDebug() << "sCan mode:" << mode;
    emit findBandsWithProfile(mode);
}

void Mainwindows::cppSubmitTextFiled(const QString &qmlJson)
{
    qDebug() << "C++: cppSubmitTextFiled:: qmlJson =" << qmlJson;

    QJsonParseError parseError;
    QJsonDocument d = QJsonDocument::fromJson(qmlJson.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << parseError.errorString();
        return;
    }

    if (!d.isObject()) {
        qWarning() << "JSON is not an object!";
        return;
    }

    QJsonObject command = d.object();
    QString getCommand = command.value("menuID").toString();

    qDebug() << "Parsed menuID:" << getCommand;
}
void Mainwindows::updateDSPOutputGain(const uint8_t value, const uint8_t outputChannel)
{
    double setValue = 1.0;
    double dbValue = 0;
    if (value == 0) dbValue = 0;
    else dbValue =  double(value/(-2));
    if (value == 0) setValue = 1;
    else {
        setValue = pow(10,dbValue/20.0);
    }
    switch (outputChannel)
    {
    case 1:
        VolumeOutDSPCH1 = setValue;
        SigmaFirmWareDownLoad->setModuleSingleVolume(AUDIOOUT_VOLUME_CH1_TARGET_ADDR,AUDIOOUT_VOLUME_CH1_MOD_ADDR,AUDIOOUT_VOLUME_CH1_MOD_VALUE,VolumeOutDSPCH1);
        break;
    case 2:
        VolumeOutDSPCH2 = setValue;
        SigmaFirmWareDownLoad->setModuleSingleVolume(AUDIOOUT_VOLUME_CH2_TARGET_ADDR,AUDIOOUT_VOLUME_CH2_MOD_ADDR,AUDIOOUT_VOLUME_CH2_MOD_VALUE,VolumeOutDSPCH2);
        break;
    case 3:
        VolumeOutDSPCH3 = setValue;
        SigmaFirmWareDownLoad->setModuleSingleVolume(AUDIOOUT_VOLUME_CH3_TARGET_ADDR,AUDIOOUT_VOLUME_CH3_MOD_ADDR,AUDIOOUT_VOLUME_CH3_MOD_VALUE,VolumeOutDSPCH3);
        break;
    case 4:
        VolumeOutDSPCH4 = setValue;
        SigmaFirmWareDownLoad->setModuleSingleVolume(AUDIOOUT_VOLUME_CH4_TARGET_ADDR,AUDIOOUT_VOLUME_CH4_MOD_ADDR,AUDIOOUT_VOLUME_CH4_MOD_VALUE,VolumeOutDSPCH4);
        break;
    }
}

void Mainwindows::updateDSPRecInputGain(int value, uint8_t softPhoneID)
{
    value = value - 12;
    double setValue = 1.0;
    double dbValue = 0;
    if (value == 0) dbValue = 0;
    else dbValue =  double(value/(-2.0));
    if (value == 0) setValue = 1;
    else {
        setValue = pow(10,dbValue/20.0);
    }
    qDebug() << "updateDSPRecInputGain" << setValue ;
    switch (softPhoneID)
    {
    case 1:
        VolumeRecInDSPCH1 = setValue;
        SigmaFirmWareDownLoad->setDSPSplitVolume(REC_IN_LEVEL_CH1_ADDR,setValue);
        break;
    case 2:
        VolumeRecInDSPCH2 = setValue;
        SigmaFirmWareDownLoad->setDSPSplitVolume(REC_IN_LEVEL_CH2_ADDR,setValue);
        break;
    case 3:
        VolumeRecInDSPCH3 = setValue;
        SigmaFirmWareDownLoad->setDSPSplitVolume(REC_IN_LEVEL_CH3_ADDR,setValue);
        break;
    case 4:
        VolumeRecInDSPCH4 = setValue;
        SigmaFirmWareDownLoad->setDSPSplitVolume(REC_IN_LEVEL_CH4_ADDR,setValue);
        break;
    }
}

void Mainwindows::updateDSPRecOutputGain(int value, uint8_t softPhoneID)
{
    value = value - 12;
    double setValue = 1.0;
    double dbValue = 0;
    if (value == 0) dbValue = 0;
    else dbValue =  double(value/(-2));
    if (value == 0) setValue = 1;
    else {
        setValue = pow(10,dbValue/20.0);
    }
    qDebug() << "updateDSPRecOutputGain" << setValue;
    switch (softPhoneID)
    {
    case 1:
        VolumeRecOutDSPCH1 = setValue;
        SigmaFirmWareDownLoad->setDSPSplitVolume(REC_OUT_LEVEL_CH1_ADDR,setValue);
        break;
    case 2:
        VolumeRecOutDSPCH2 = setValue;
        SigmaFirmWareDownLoad->setDSPSplitVolume(REC_OUT_LEVEL_CH2_ADDR,setValue);
        break;
    case 3:
        VolumeRecOutDSPCH3 = setValue;
        SigmaFirmWareDownLoad->setDSPSplitVolume(REC_OUT_LEVEL_CH3_ADDR,setValue);
        break;
    case 4:
        VolumeRecOutDSPCH4 = setValue;
        SigmaFirmWareDownLoad->setDSPSplitVolume(REC_OUT_LEVEL_CH4_ADDR,setValue);
        break;
    }
}

void Mainwindows::updateDSPSpeakerOutputGain(int value, uint8_t softPhoneID)
{
    qDebug() << "value:" << value;
    // value = value - 12;
    double setValue = 1.0;
    // double dbValue = 0;
    // if (value == 0) dbValue = 0;
    // else dbValue =  double(value/(-2));
    // if (value == 0) setValue = 1;
    // else {
    //     setValue = pow(10,dbValue/20.0);
    // }

    // if (value < 0.0)   value = 0.0;
    // if (value > 205.0) value = 205.0;

    // setValue =static_cast<int>((205.0 - value) * 255.0 / 205.0);

    // CODEC_PCM3168A->setOutputGain(CODECCH1_I2S1,CODECVolumeOutCH1);
    // CODEC_PCM3168A->setOutputGain(CODECCH2_I2S1,CODECVolumeOutCH2);
    // CODEC_PCM3168A->setOutputGain(CODECCH3_I2S1,CODECVolumeOutCH3);
    // CODEC_PCM3168A->setOutputGain(CODECCH4_I2S1,CODECVolumeOutCH4);

    if (value < 0.0)   value = 0.0;
    if (value > 205.0) value = 205.0;

    setValue = static_cast<double>((205.0 - value) * 255.0 / 205.0);

    qDebug() << "updateDSPSpeakerOutputGain" << setValue << " default:" << value << " softPhoneID::" << softPhoneID;
    switch (softPhoneID)
    {
    case 1:
        VolumeRecOutDSPCH1 = setValue;
        CODEC_PCM3168A->setOutputGainWithoutDSP(CODECCH1_I2S1,setValue);
        // SigmaFirmWareDownLoad->setDSPSplitVolume(REC_OUT_LEVEL_CH1_ADDR+1,setValue);
        break;
    case 2:
        VolumeRecOutDSPCH2 = setValue;
        CODEC_PCM3168A->setOutputGainWithoutDSP(CODECCH2_I2S1,setValue);
        CODEC_PCM3168A->setOutputGainWithoutDSP(CODECCH3_I2S1,setValue);
        // SigmaFirmWareDownLoad->setDSPSplitVolume(REC_OUT_LEVEL_CH2_ADDR+1,setValue);
        break;
    case 3:
        VolumeRecOutDSPCH3 = setValue;
        CODEC_PCM3168A->setOutputGainWithoutDSP(CODECCH3_I2S1,setValue);
        // SigmaFirmWareDownLoad->setDSPSplitVolume(REC_OUT_LEVEL_CH3_ADDR+1,setValue);
        break;
    case 4:
        VolumeRecOutDSPCH4 = setValue;
        CODEC_PCM3168A->setOutputGainWithoutDSP(CODECCH4_I2S1,setValue);
        CODEC_PCM3168A->setOutputGainWithoutDSP(CODECCH5_I2S1,setValue);
        // SigmaFirmWareDownLoad->setDSPSplitVolume(REC_OUT_LEVEL_CH4_ADDR+1,setValue);
        break;
    }
}

void Mainwindows::newCommandProcessWeb(const QJsonObject command, QWebSocket *pSender, const QString &message){
    QString objectName =  QJsonValue(command["objectName"]).toString();
    QString menuID     = command.value("menuID").toString().trimmed();
    qDebug() << "newCommandProcessWeb" << command;
    // =========================================================
    // ✅ NEW: handle commands that come without objectName
    // =========================================================
    if(objectName == "update"){
        // qDebug() << "message" << message;
        emit updateListProfiles();
    }
    else if(objectName == "deletePreset"){
        QString id = QJsonValue(command["presetId"]).toString();
        // qDebug() << "deletePreset:" << message << " id:::" << id;
        emit deleteSpecificProfile(id);
    }
    else if(objectName == "deletePresetAll"){
        qDebug() << "deletePresetAll:" << message;
        emit deleteAllPresets();
    }
    else if(objectName == "deleteScanPreset"){
        QString id = QJsonValue(command["presetId"]).toString();
        qDebug() << "deleteScanPreset:" << message << " id:::" << id;
        deleteScanCardSlot(id);
    }
    else if(objectName == "deleteScanGroup"){
        // deleteScanCardAllSlot();
        QString groupKeyUtc = command.value("groupKey").toString(); // "2025-12-04T08:25:39.000Z"
        QString groupKeyThai = toThaiTimeString(groupKeyUtc);   // "2025-12-04 15:25:39"
        QString groupKeyThaiWithoutT = toThaiTimeStringWithoutT(groupKeyUtc);   // "2025-12-04 15:25:39"
        qDebug() << "deleteGroupScan:" << message << " groupKeyThai:" << groupKeyThai << " groupKeyThaiWithoutT:" << groupKeyThaiWithoutT;
        // for(ScanCard s : cards){
        //     qDebug() << "deleteScanGroup before ScanCard::" << s.id << s.time;
        // }
        emit deleteScanCardGroup(groupKeyThai);
        deleteScanGroupByKey(groupKeyThaiWithoutT);
    }
    else if(objectName == "deleteScanAll"){
        deleteScanCardAllSlot();
        qDebug() << "deleteScanCardAll:" << message;
    }
    else if(objectName == "editPreset"){
        QString id = QJsonValue(command["presetId"]).toString();
        // qDebug() << "editPreset:" << message << " id:::" << id;
        emit editSpecificProfile(message);
    }
    else if(objectName == "selectSpecificProfile"){
        QJsonObject temp = command;
        temp.remove("objectName");
        temp.remove("menuID");
        qDebug() << "selectSpecificProfile:" << temp;
        emit selectSpecificProfile(temp);
    }
    else if(objectName == "newPreset"){
        QJsonObject temp = command;
        temp.remove("objectName");
        temp.remove("menuID");
        emit addNewProfile(temp);
    }
    else if(objectName == "Scan"){
        qDebug() << "Scan:::" << message;
        sCan(message);
    }
    else{
        qDebug() << "else:::" << message;
    }
}

QString Mainwindows::toThaiTimeStringWithoutT(const QString &isoUtc)
{
    // isoUtc เช่น "2025-12-04T08:25:39.000Z"
    QDateTime dtUtc = QDateTime::fromString(isoUtc, Qt::ISODate);
    dtUtc.setTimeSpec(Qt::UTC);   // บอกให้ Qt รู้ว่าเป็นเวลา UTC

    // แปลงเป็นโซนเวลาไทย (Asia/Bangkok, UTC+7)
    QDateTime dtThai = dtUtc.toTimeZone(QTimeZone("Asia/Bangkok"));

    // ฟอร์แมตเป็น "2025-12-04 15:25:39"
    return dtThai.toString("yyyy-MM-dd HH:mm:ss");
}

QString Mainwindows::toThaiTimeString(const QString &isoUtc)
{
    // isoUtc เช่น "2025-12-04T08:25:39.000Z"
    QDateTime dtUtc = QDateTime::fromString(isoUtc, Qt::ISODate);
    dtUtc.setTimeSpec(Qt::UTC);   // บอกให้ Qt รู้ว่าเป็นเวลา UTC

    // แปลงเป็นโซนเวลาไทย (Asia/Bangkok, UTC+7)
    QDateTime dtThai = dtUtc.toTimeZone(QTimeZone("Asia/Bangkok"));

    // ฟอร์แมตเป็น "2025-12-04 15:25:39"
    return dtThai.toString("yyyy-MM-ddTHH:mm:ss");
}

void Mainwindows::deleteCardWebSlot(QString val) {
    qDebug() << "sendUpdateWebSlot" << val;

    QJsonObject Param;
    Param["objectName"] = "deletePreset";
    Param["presetId"] = val;
    // แปลงเป็น JSON string แบบถูกต้อง
    QString raw_data = QString::fromUtf8(QJsonDocument(Param).toJson(QJsonDocument::Compact));

    // qDebug() << "Broadcast:" << raw_data;

    // ส่งให้ webServer
    if (webServer) {
        webServer->broadcastMessage(raw_data);
    }
}

void Mainwindows::addCardWebSlot(QString val) {
    qDebug() << "sendUpdateWebSlot" << val;

    QJsonObject Param;
    Param["objectName"] = "addPreset";
    Param["presetId"] = val;
    // แปลงเป็น JSON string แบบถูกต้อง
    QString raw_data = QString::fromUtf8(QJsonDocument(Param).toJson(QJsonDocument::Compact));

    // qDebug() << "Broadcast:" << raw_data;

    // ส่งให้ webServer
    if (webServer) {
        webServer->broadcastMessage(raw_data);
    }
}

void Mainwindows::editCardWebSlot(QString presetId, QString msg)
{
    QByteArray br = msg.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);
    QJsonObject obj = doc.object();

    qDebug() << "editCardWebSlot" << presetId << " obj::" << obj;

    // ----- Local variables -----
    QString name            = obj.value("name").toString();
    QString mod             = obj.value("mod").toString();

    double center_freq      = obj.value("center_freq").toDouble();
    double offset_freq      = obj.value("offset_freq").toDouble();
    double low_cut          = obj.value("low_cut").toDouble();
    double high_cut         = obj.value("high_cut").toDouble();

    int audio_service_id    = obj.value("audio_service_id").toInt();
    int dmr_filter          = obj.value("dmr_filter").toInt();

    double squelch_level    = obj.value("squelch_level").toDouble();
    bool secondary_mod      = obj.value("secondary_mod").toBool();

    // ----- Build JSON to send to Web ------
    QJsonObject Param;
    Param["objectName"]     = "editPreset";
    Param["presetId"]       = presetId;
    Param["name"]           = name;
    Param["mod"]            = mod;
    Param["center_freq"]    = center_freq;
    Param["offset_freq"]    = offset_freq;
    Param["low_cut"]        = low_cut;
    Param["high_cut"]       = high_cut;
    Param["audio_service_id"] = audio_service_id;
    Param["dmr_filter"]       = dmr_filter;
    Param["secondary_mod"]    = secondary_mod;
    Param["squelch_level"]    = squelch_level;

    QString raw_data = QString::fromUtf8(
        QJsonDocument(Param).toJson(QJsonDocument::Compact)
        );

    // ส่งให้เว็บ
    if (webServer) {
        qDebug() << "editCardWebSlot webServer" << raw_data;
        webServer->broadcastMessage(raw_data);
    }
}

void Mainwindows::profileWeb(QString msg)
{
    // cards.clear();
    // cardsProfilesArray = QJsonArray();
    QByteArray br = msg.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);
    if (!doc.isObject()) {
        qDebug() << "[profileWeb] Invalid JSON object";
        return;
    }

    QJsonObject obj = doc.object();
    QString objectName = obj.value("objectName").toString();
    QJsonArray arr = obj.value("profiles").toArray();

    if (objectName == "profilesCard") {

        // ล้างของเดิมก่อน (กันข้อมูลค้าง)
        scanrf->freq.clear();
        scanrf->bw.clear();
        scanrf->mod.clear();
        // ถ้ามี struct สำหรับ low/high ใน scanrf ด้วย ก็ clear ตรงนี้เพิ่มได้
        // scanrf->low_cut.clear();
        // scanrf->high_cut.clear();

        // ---------- เตรียม JSON สำหรับเซฟลงไฟล์ ----------
        QJsonArray profilesArray;

        QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
        // ---------- วนลูปเก็บลง struct + เตรียม JSON ----------
        for (int i = 0; i < arr.size(); i++) {
            QJsonObject o = arr.at(i).toObject();

            int index        = o.value("index").toInt();
            double freq      = o.value("frequency").toDouble();
            QString unit     = o.value("unit").toString();
            QString bw       = o.value("bw").toString();
            double startHz   = o.value("startHz").toDouble();
            double endHz     = o.value("endHz").toDouble();
            QString mode     = o.value("mode").toString();
            QString time     = o.value("time").toString();

            // ===== NEW: อ่าน low_cut / high_cut จาก JSON (มาจาก peakScan.lowCutNow / highCutNow) =====
            int lowCut       = o.value("low_cut").toInt();   // ถ้าไม่มี field จะได้ 0
            int highCut      = o.value("high_cut").toInt();

            // เก็บลงโครงสร้างเดิมที่คุณใช้
            scanrf->freq.push_back(freq);
            scanrf->bw.append(bw);
            scanrf->mod.append(mode);

            // ถ้ามี field low/high ใน struct ของ scanrf ก็เก็บเพิ่มได้ เช่น:
            // scanrf->low_cut.push_back(lowCut);
            // scanrf->high_cut.push_back(highCut);


            // สร้าง object สำหรับไฟล์ JSON
            ScanCard c;
            QJsonObject jprof;
            jprof["index"]     = index;
            jprof["frequency"] = freq;
            jprof["unit"]      = unit;
            jprof["bw"]        = bw;
            jprof["startHz"]   = startHz;
            jprof["endHz"]     = endHz;
            jprof["mode"]      = mode;
            jprof["time"]      = now;

            // ===== NEW: เขียน low_cut / high_cut ลงไฟล์ด้วย =====
            jprof["low_cut"]   = lowCut;
            jprof["high_cut"]  = highCut;

            c.id = index;
            c.freq = freq;
            c.unit = unit;
            c.bw = bw;
            c.high_cut = highCut;
            c.low_cut = lowCut;
            c.mode = mode;
            c.time = QDateTime::fromString(
                now,
                Qt::ISODate);

            cards.append(c);
            profilesArray.append(jprof);
            cardsProfilesArray.append(jprof);
            qDebug() << "profileWeb cards.append(c):" << cards.size();

            // เวลาแบบ ISO8601

            c.time = QDateTime::fromString(
                now,
                Qt::ISODate);
            qDebug() << "insertScanCard freq:" << freq;
            emit insertScanCard(freq,unit,bw,mode,lowCut,highCut,"/var/lib/openwebrx/scanpreset.json",now);
        }

        // ---------- เขียน JSON ลงไฟล์ /var/lib/openwebrx/scanpreset.json ----------
        QString outPath = "/var/lib/openwebrx/scanpreset.json";
        QFile outFile(outPath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qDebug() << "[profileWeb] Cannot open file for write:" << outPath;
        } else {
            // QJsonObject root;
            // root["profiles"] = profilesArray;

            // QJsonDocument outDoc(root);
            // outFile.write(outDoc.toJson(QJsonDocument::Indented));
            // outFile.close();

            qDebug() << "[profileWeb] Saved profiles JSON to:" << outPath;
        }


        // QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
        qDebug() << "::arr::" << arr;
        startScanCard->start(3000);
        // initValueJson(profilesArray);
        qDebug() << "freq size:" << scanrf->freq.size()
                 << " bw size:"  << scanrf->bw.size()
                 << " mod size:" << scanrf->mod.size();

        // ---------- ส่งข้อมูลที่ได้กลับไปยังเว็บ ----------
        // if (webServer) {
        //     QJsonObject sendObj;
        //     sendObj["objectName"] = "profilesCard";
        //     sendObj["profiles"]   = profilesArray;

        //     QJsonDocument sendDoc(sendObj);
        //     QString raw_data = QString::fromUtf8(sendDoc.toJson(QJsonDocument::Compact));

        //     webServer->broadcastMessage(raw_data);
        //     qDebug() << "[profileWeb] Broadcast profilesCard to web, bytes:" << raw_data.size();
        // } else {
        //     qDebug() << "[profileWeb] webServer is null, cannot broadcast profiles";
        // }
    }
}

////////////////////////////////////////SCAN CARDS//////////////////////////////////////////////

void Mainwindows::initValueJson(const QJsonArray &arr)
{
    // เคลียร์ของเก่าก่อน กันซ้ำ
    // cards.clear();
    // cardsProfilesArray = QJsonArray();
    qDebug() << "[Mainwindows] JSON array size =" << arr.size();
    for (int i = 0; i < arr.size(); i++) {

        QJsonObject o = arr.at(i).toObject();
        qDebug() << "[initValueJson]" << o.value("time").toString();
        // jprof["index"]     = index;
        // jprof["frequency"] = freq;
        // jprof["unit"]      = unit;
        // jprof["bw"]        = bw;
        // jprof["startHz"]   = startHz;
        // jprof["endHz"]     = endHz;
        // jprof["mode"]      = mode;

        // // ===== NEW: เขียน low_cut / high_cut ลงไฟล์ด้วย =====
        // jprof["low_cut"]   = lowCut;
        // jprof["high_cut"]  = highCut;
        ScanCard row;
        row.id         = o.value("id").toInt();
        row.freq       = o.value("frequency").toDouble();
        row.unit       = o.value("unit").toString();
        row.bw         = o.value("bw").toString();
        row.mode       = o.value("mode").toString();
        row.low_cut    = o.value("low_cut").toInt();
        row.high_cut   = o.value("high_cut").toInt();
        row.path       = o.value("path").toString();
        row.time = QDateTime::fromString(
            o.value("time").toString(),
            Qt::ISODate);


        QJsonObject jprof;
        jprof["index"]     = o.value("id").toInt();
        jprof["frequency"] = o.value("frequency").toDouble();
        jprof["unit"]      = o.value("unit").toString();
        jprof["bw"]        = o.value("bw").toString();
        jprof["startHz"]   = 0;
        jprof["endHz"]     = 0;
        jprof["mode"]      = o.value("mode").toString();

        // ===== NEW: เขียน low_cut / high_cut ลงไฟล์ด้วย =====
        jprof["low_cut"]   = o.value("low_cut").toInt();
        jprof["high_cut"]  = o.value("high_cut").toInt();
        jprof["time"]  = o.value("time").toString();

        cardsProfilesArray.append(jprof);
        cards.append(row);
        qDebug() << "initValueJson cards.append(row):" << cards.size();
    }
    startScanCard->start(3000);
    // for (const ScanCard &c : cards) {
    //     qDebug() << "initValueJson array:" << c.id << c.freq << c.mode;
    // }
    // จะส่งไป WebSocket ก็ทำตรงนี้ได้เลย
}

void Mainwindows::startScanCardFn(){
    qDebug() << "startScanCardFn size::" << cardsProfilesArray.size();
    QVariantList list = cardsProfilesArray.toVariantList();
    emit profilesFromDb(list);

    if (webServer) {
        QJsonObject sendObj;
        sendObj["objectName"] = "profilesCard";
        sendObj["profiles"]   = cardsProfilesArray;

        QJsonDocument sendDoc(sendObj);
        QString raw_data = QString::fromUtf8(sendDoc.toJson(QJsonDocument::Compact));

        webServer->broadcastMessage(raw_data);
        qDebug() << "[profileWeb] Broadcast profilesCard to web, bytes:" << raw_data.size();
    } else {
        qDebug() << "[profileWeb] webServer is null, cannot broadcast profiles";
    }
}

QJsonArray Mainwindows::removeCardById(const QJsonArray &array, int targetId)
{
    QJsonArray newArray;

    for (const QJsonValue &val : array) {
        if (!val.isObject())
            continue;

        QJsonObject obj = val.toObject();

        // ไม่ใช่ id ที่ต้องลบ → เก็บไว้
        if (obj.value("index").toInt() != targetId) {
            newArray.append(obj);
        }
    }

    qDebug() << "removeCardById:" << newArray;
    return newArray;
}

void Mainwindows::deleteScanCardSlot(QString id){
    qDebug() << "deleteScanCardSlot" << id << cards.size();
    // for(ScanCard s : cards){
    //     qDebug() << "before ScanCard::" << s.id;
    // }
    int index = QString(id).toInt();
    // qInfo() << "[deleteScanCardById] Removed id =" << id << "at index" << index << " cards_size:" << cards.size();
    if(cards.isEmpty()){
        qDebug() << "Cards isEmpty" << cards.size();
        return;
    }
    for (int i = 0; i < cards.size(); i++) {
        if (cards[i].id == index) {
            cards.removeAt(i);
            emit deleteScanCardById(index);
            QJsonArray newArray = removeCardById(cardsProfilesArray,index);
            cardsProfilesArray = newArray;
            // qInfo() << "[i < cards.size();] Removed id =" << id << "at index" << index << " count:" << i;
        }
    }

    if (webServer) {
        QJsonObject sendObj;
        sendObj["objectName"] = "ScanCardCount";
        sendObj["ScanCardCount"] = cards.size();

        QJsonDocument sendDoc(sendObj);
        QString raw_data = QString::fromUtf8(sendDoc.toJson(QJsonDocument::Compact));

        webServer->broadcastMessage(raw_data);
        // cardsProfilesArray = QJsonArray();
        qDebug() << "[profileWeb] Broadcast ScanCardCount to web, bytes:" << raw_data.size();
    } else {
        qDebug() << "[profileWeb] webServer is null, cannot broadcast ScanCardCount";
    }

    if (webServer) {
        QJsonObject sendObj;
        sendObj["objectName"] = "deleteScan";
        sendObj["presetId"] = id;

        QJsonDocument sendDoc(sendObj);
        QString raw_data = QString::fromUtf8(sendDoc.toJson(QJsonDocument::Compact));

        webServer->broadcastMessage(raw_data);
        // cardsProfilesArray = QJsonArray();
        qDebug() << "[profileWeb] Broadcast ScanCardCount to web, bytes:" << raw_data.size();
    } else {
        qDebug() << "[profileWeb] webServer is null, cannot broadcast ScanCardCount";
    }

    emit scanCardUpdateDelete(id.toInt());
    QVariantList list = cardsProfilesArray.toVariantList();
    emit profilesFromDb(list);
}

void Mainwindows::deleteScanCardAllSlot(){
    cards.clear();
    cardsProfilesArray = QJsonArray();
    emit deleteScanCardAll();
    if (webServer) {
        QJsonObject sendObj;
        sendObj["objectName"] = "deleteScanAll";

        QJsonDocument sendDoc(sendObj);
        QString raw_data = QString::fromUtf8(sendDoc.toJson(QJsonDocument::Compact));

        webServer->broadcastMessage(raw_data);
        // cardsProfilesArray = QJsonArray();
        qDebug() << "[profileWeb] Broadcast profilesCard to web, bytes:" << raw_data.size();
    } else {
        qDebug() << "[profileWeb] webServer is null, cannot broadcast profiles";
    }
    QVariantList list = cardsProfilesArray.toVariantList();
    emit profilesFromDb(list);
}

void Mainwindows::deleteScanGroupByKey(const QString &groupKeyThai)
{
    // ---------------------------
    // 1) ลบจาก QVector<ScanCard> cards
    // ---------------------------
    // ลบแบบวนถอยหลังเพื่อไม่ให้ index เลื่อนทับกัน
    for (int i = cards.size() - 1; i >= 0; --i) {
        const QDateTime &dt = cards[i].time;   // หรือ cards[i].time ถ้าใช้ field นั้นเป็น group

        if (!dt.isValid())
            continue;

        // แปลง QDateTime -> "yyyy-MM-dd HH:mm:ss" เพื่อเทียบกับ groupKeyThai
        const QString dtStr = dt.toString("yyyy-MM-dd HH:mm:ss");

        // qDebug() << "deleteScanGroupByKey::" << dtStr << groupKeyThai;
        if (dtStr == groupKeyThai) {
            qDebug() << "deleteScanGroupByKey::" << dtStr << groupKeyThai << " removeAt:" << i;
            // เวลา match → ลบทั้ง index นี้ทิ้ง
            cards.removeAt(i);
        }
    }

    // ---------------------------
    // 2) ลบจาก QJsonArray cardsProfilesArray
    // ---------------------------
    QJsonArray newArray;

    for (const QJsonValue &v : cardsProfilesArray) {
        if (!v.isObject()) {
            newArray.append(v);
            continue;
        }

        QJsonObject obj = v.toObject();

        // สมมติว่าเก็บเวลาไว้ใน field "groupKey" หรือ "created_at"
        const QString keyStr      = obj.value("groupKey").toString();
        const QString createdStr  = obj.value("time").toString();
        QString createdStrFixed = createdStr;
        createdStrFixed.replace("T", " ");

        // ถ้า field ใด field หนึ่ง match groupKeyThai → ข้าม (ไม่เก็บเข้า newArray)
        if (keyStr == groupKeyThai || createdStrFixed == groupKeyThai) {
            continue;
        }
        // qDebug() << "keyStr:" << keyStr << " createdStrFixed:" << createdStrFixed << " groupKeyThai:" << groupKeyThai;
        // ไม่ match → เก็บไว้
        newArray.append(obj);
    }

    cardsProfilesArray = newArray;

    QVariantList list = cardsProfilesArray.toVariantList();
    emit profilesFromDb(list);

    qInfo() << "[deleteScanGroupByKey] Removed items with time =" << groupKeyThai
            << " -> cards size =" << cards.size()
            << ", cardsProfilesArray size =" << cardsProfilesArray.size();
}

void Mainwindows::setLocation(QString location){
    if (!location.contains("Select")){
        QString command = QString("ln -sf /usr/share/zoneinfo/%1  /etc/localtime").arg(location);
        system(command.toStdString().c_str());
        timeLocation = location;
    }
}

void* Mainwindows::ThreadFuncSqlWatcher(void* pTr)
{
    Mainwindows* pThis = static_cast<Mainwindows*>(pTr);

    while (pThis->m_threadRunning) {

        const bool v = pThis->m_lastRecIsRecord;

        QMetaObject::invokeMethod(pThis, [pThis, v]() {
            emit pThis->onRecStatusChanged(v);

            // ถ้าคุณอยากให้ SQL ถูกยิงซ้ำด้วย
            if (pThis->m_lastRecState == "RECORD" && pThis->currentSQLValue == false) {
                pThis->onSQLChanged(pThis->currentSQLValue);
            }
        }, Qt::QueuedConnection);

        QThread::msleep(300);
    }
    return nullptr;
}

