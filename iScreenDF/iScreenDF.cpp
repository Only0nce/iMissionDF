#include "iScreenDF.h"

#define SET_AND_EMIT(var, val, signal) \
if ((var) != (val)) { (var) = (val); emit signal(); }

iScreenDF::iScreenDF(ImageProviderDF *imageProvider, QObject *parent)
    : QObject(parent)
{
    system("hwclock -s");
    system("noblank.sh");
    system("sudo rm /var/www/html/uploads/*");

    chatServerDF  = new ChatServerDF(8000);
    chartclientDF = new ChatClientDF(this);
    localDFclient = new TcpClientDF(this);
    keepAliveTimer = new QTimer(this);
    compassTimer = new QTimer(this);

    QThread::msleep(200);

    networks         = new Network;
    netServerKraken  = new NetworkServerKraken;
    krakenparameter  = new Krakenparameter;
    capture          = imageProvider;

    networking = new NetworkMng();
    reConnect  = new QTimer();

    // ====== Database + Thread ======
    dbThread = new QThread(this);

    ///////////////////////// GPS Info //////////////////////////////////////
    qRegisterMetaType<GPSInfo>("GPSInfo");

    gpsReader = new GpsdReader(this);
    gpsReader->setDeviceMap("/dev/ttyS3", QString());

    connect(gpsReader, &GpsdReader::gps1Updated,
            this, &iScreenDF::gps1Updated);

    connect(gpsReader, &GpsdReader::gps2Updated,
            this, &iScreenDF::gps2Updated);

    connect(gpsReader, &GpsdReader::errorOccurred,
            this, [](const QString &msg){
                // qWarning() << "[GpsdReader ERROR]" << msg;
            });

    connect(gpsReader, &GpsdReader::gpsdConnected,
            this, [](bool ok){
                // qDebug() << "[GpsdReader] gpsdConnected =" << ok;
            });

    gpsReader->start();
    ////////////////////////////// GPSInfo END /////////////////////////////////
    // connectCompassServer("192.168.10.85", 5000);
    connectCompassServer("127.0.0.1", 5000);


    db = new DatabaseDF("iScreen","orinnx","Ifz8zean6868**","127.0.0.1",nullptr); // orinnx iScreenKraken

    db->moveToThread(dbThread);
    connect(dbThread, &QThread::started,db,&DatabaseDF::init);
    connect(dbThread, &QThread::finished,db,&QObject::deleteLater);
    connect(qApp,&QCoreApplication::aboutToQuit, this, [this]{ QMetaObject::invokeMethod(db, "shutdown", Qt::QueuedConnection);
        if (dbThread) {
            dbThread->quit();
            dbThread->wait();
        }
    });
    // start db thread
    dbThread->start();

    // ====== TCP Server (QTcpServer) ======
    tcpServerDF = new TcpServerDF(9000, this);
    connect(tcpServerDF, &TcpServerDF::messageReceived,this,&iScreenDF::onTcpMessage);
    connect(tcpServerDF, &TcpServerDF::clientConnected,this,&iScreenDF::onTcpClientConnected);
    connect(tcpServerDF, &TcpServerDF::clientDisconnected,this,&iScreenDF::onTcpClientDisconnected);

    // ====== WebSocket / signals  ======
    connect(chatServerDF, &ChatServerDF::newCommandProcess,this,&iScreenDF::newCommandProcess);


    // iScreenDF (main thread) → Database (dbThread)
    // connect(this, &iScreenDF::setNetwork,db,&Database::setNetworkSlot,Qt::QueuedConnection);
    // connect(this, &iScreenDF::updateKrakenServer,db,&Database::updateKrakenServer,Qt::QueuedConnection);

    // Database (dbThread) → iScreenDF (main thread)
    connect(db, &DatabaseDF::updateNetwork,this, &iScreenDF::updateNetworkSlot, Qt::QueuedConnection);
    connect(db, &DatabaseDF::updateNTPServer,this, &iScreenDF::updateNTPServerSlot, Qt::QueuedConnection);
    // connect(db, &DatabaseDF::updateNetworkServerKraken,this, &iScreenDF::updateNetworkServerKraken, Qt::QueuedConnection);
    // connect(db, &DatabaseDF::setConnectToserverKraken,this, &iScreenDF::setConnectToserverKraken, Qt::QueuedConnection);
    connect(db, &DatabaseDF::remoteGroupsJson,this, &iScreenDF::remoteGroupsJson, Qt::QueuedConnection);
    connect(db, &DatabaseDF::remoteSideRemoteJson,this, &iScreenDF::remoteSideRemoteJson, Qt::QueuedConnection);
    connect(db, &DatabaseDF::sigGroupsInGroupSetting,this, &iScreenDF::sigGroupsInGroupSetting, Qt::QueuedConnection);
    connect(db, &DatabaseDF::NetworkAppen,this, &iScreenDF::NetworkAppen, Qt::QueuedConnection);

    // connect(db, &Database::appendNewClient,this, &iScreenDF::appendNewClient, Qt::QueuedConnection);
    connect (db, &DatabaseDF::appendNewActiveClient, this , &iScreenDF::appendNewActiveClient, Qt::QueuedConnection);
    // connect(db, &Databases::)
    // ImageProvider
    // connect(capture, &ImageProvider::sendpartImage,this, &iScreenDF::sendpartImage);
    connect(db, &DatabaseDF::devicesInGroupJsonReady,this, &iScreenDF::DevicesInGroupJsonReady);
    connect(db, &DatabaseDF::setupServerClientForDevices,this, &iScreenDF::setupServerClientForDevices);
    // recoreder
    connect( db, &DatabaseDF::recorderSettingsReady,this, &iScreenDF::recorderSettingsReady);
    // parameter
    connect(db, &DatabaseDF::parameterReceived, this, &iScreenDF::parameterReceived);

    connect(db, &DatabaseDF::Getrfsocparameter,this, &iScreenDF::GetrfsocParameter, Qt::QueuedConnection);
    connect(db, &DatabaseDF::GetIPDFServer,this, &iScreenDF::GetIPDFServer, Qt::QueuedConnection);
    connect(db, &DatabaseDF::updateNetworkDfDevice,
            this,       &iScreenDF::onUpdateNetworkDfDevice, Qt::QueuedConnection);
    // ====== ห้ามเรียก db->xxx ตรง ๆ ตรงนี้แล้ว! ======
    // db->ensureColumnsInIScreenparameter();
    // db->createServerKrakenNetworkTable();
    // db->getNetwork();
    // db->getNTPServer();
    // db->getServerKrakenNetwork();
    // db->getNetworkfromDb();
    // db->getAllClientInDatabase();
    // ทั้งหมดนี้ย้ายไปอยู่ใน Database::init()

    connect(keepAliveTimer, &QTimer::timeout, this, [this]() {
        QJsonObject keepAliveObj;
        keepAliveObj["menuID"] = "keepAlive";
        QString keepAliveJson = QString::fromUtf8(
            QJsonDocument(keepAliveObj).toJson(QJsonDocument::Compact)
            );
        // Loop ทุก group และ client
        Q_FOREACH (groupActive *group, group_active_list)
        {
            if (!group) continue;

            Q_FOREACH (clientNode *client, group->client_active_list)
            {
                if (!client || !client->chatclient) continue;

                if (client->chatclient->isConnected)
                {
                    client->chatclient->m_webSocket->sendTextMessage(keepAliveJson);
                    qDebug() << "[KEEPALIVE] Sent to" << client->ipAddress;
                }
            }
        }
    });

    //////////////////////////////////////////////////////////////////////////////////
    // localDFclient->connectToServer("192.168.10.87",5555);

    connect(localDFclient, &TcpClientDF::connected,
            this, &iScreenDF::sendParameterToServer);

    connect(localDFclient, &TcpClientDF::disconnected,
            this, [](){
                qDebug() << "DOA TCP Disconnected";
            });

    connect(localDFclient, &TcpClientDF::errorOccurred,
            this, [](const QString &err){
                qDebug() << "[DOA ERROR]" << err;
            });

    // connect(localDFclient, &TcpClientDF::connected,
    //         this, [this]() {

    //             QJsonObject param{
    //                               {"menuID","getState"},
    //                               {"needAck",true},
    //                               };

    //             const QByteArray line = QJsonDocument(param).toJson(QJsonDocument::Compact);

    //             qDebug() << "[DF] connected -> send:" << line;

    //             localDFclient->sendLine(line, true);
    //         });
    // connect(localDFclient, &TcpClientDF::doaResultReceived,
    //         this, &iScreenDF::onDoAResultReceived);

    // connect(compassTimer, &QTimer::timeout, this, [this]() {
    //     QJsonObject single;
    //     single["objectName"] = "ReadDirection";
    //     QString sendJson = QString::fromUtf8(QJsonDocument(single).toJson(QJsonDocument::Compact));
    //     // chatServer->broadcastMessage(sendJson);
    //     m_compassClient->sendJson(single);
    //     // qDebug() << "Sending compass data";
    // });
    connect(localDFclient, &TcpClientDF::updateFromTcpServer,
            this, &iScreenDF::updateFromTcpServer);
    // ====== pthread ======
    int ret = pthread_create(&idThread, nullptr, ThreadFunc, this);
    if (ret == 0) {
        qDebug() << ("Thread created successfully.\n");
    } else {
        qDebug() << ("Thread not created.\n");
    }

    qDebug() << "test gpioInit";
    gpioInit();
    keepAliveTimer->start(2000);
}

iScreenDF::~iScreenDF(){
    // if (reConnect) {
    //     reConnect->stop();
    //     delete reConnect;
    // }
    if (dbThread) {
        QMetaObject::invokeMethod(db, "shutdown", Qt::QueuedConnection);
        dbThread->quit();
        dbThread->wait();
        // myDatabase จะถูก deleteLater ตอน thread finished
    }
}

///////////////////////// GPSInfo ////////////////////////////////////////////////////
QString iScreenDF::makeGpsJson(const QString& port, const GPSInfo& info) const
{
    auto finite = [](double v, double def=0.0){ return std::isfinite(v) ? v : def; };

    QJsonObject obj;
    obj["menuID"]     = "GPS_Data";
    obj["GPSD_Port"]  = port;
    obj["GPS_Lat"]    = finite(info.lat);
    obj["GPS_Long"]   = finite(info.lon);
    obj["GPS_Alt"]    = finite(info.alt);
    obj["GPS_SatUse"] = info.satUse;
    obj["GPS_Sat"]    = info.sat;
    obj["GPS_Date"]   = info.date;
    obj["GPS_Time"]   = info.time;
    obj["GPS_Locked"] = (info.locked == 1);

    if (!info.constelCounts.isEmpty()) {
        QJsonObject cc;
        for (auto it = info.constelCounts.cbegin(); it != info.constelCounts.cend(); ++it)
            cc.insert(it.key(), it.value());
        obj.insert("Constellations", cc);
    }

    if (!info.sats.isEmpty()) {
        QJsonArray satsArr;
        for (const auto &s : info.sats) {
            QJsonObject si;
            si["prn"]     = s.prn;
            si["gnssid"]  = s.gnssid;
            si["svid"]    = s.svid;
            si["snr"]     = finite(s.snr);
            si["elev"]    = finite(s.elev);
            si["az"]      = finite(s.az);
            si["used"]    = s.used;
            si["constel"] = s.constel;
            satsArr.append(si);
        }
        obj.insert("Satellites", satsArr);
    }

    QJsonDocument doc(obj);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

void iScreenDF::updatePpsCtl()
{
    // ตัวอย่าง logic เลือก best_pps จาก state1_/state2_
    // ตอนนี้แค่ log ให้ดู
    bool newBest = (state1_.satUse >= state2_.satUse);
    if (newBest != best_pps) {
        best_pps = newBest;
        qDebug() << "best_pps" << (best_pps ? "PPS1" : "PPS2");
        // ถ้าคุณมีตัว control GPIO สำหรับเลือก PPS ให้เซตตรงนี้
    }
}
static void latLonToUTM(double lat, double lon, QString &zoneStr, QString &eStr, QString &nStr)
{
    int zone = 0;
    bool northp = true;
    double x = 0.0, y = 0.0;

    GeographicLib::UTMUPS::Forward(lat, lon, zone, northp, x, y);

    zoneStr = QString::number(zone) + (northp ? "N" : "S");
    eStr    = QString::number(x, 'f', 3);
    nStr    = QString::number(y, 'f', 3);
}

static void latLonToUTMAndMGRS(double lat, double lon, QString &utmStr, QString &mgrsStr, int mgrsPrecision = 5)
{
    int zone = 0;
    bool northp = true;
    double x = 0.0, y = 0.0;

    GeographicLib::UTMUPS::Forward(lat, lon, zone, northp, x, y);

    utmStr = QString("%1%2 E %3 N %4")
                 .arg(zone)
                 .arg(northp ? "N" : "S")
                 .arg(QString::number(x, 'f', 3))
                 .arg(QString::number(y, 'f', 3));
    // MGRS
    std::string mgrs;
    GeographicLib::MGRS::Forward(zone, northp, x, y, mgrsPrecision, mgrs);
    mgrsStr = QString::fromStdString(mgrs);
}

void iScreenDF::gps1Updated(const GPSInfo &info)
{
    const bool manual = m_manualGps1.enabled;
    const bool good = (!manual && isGoodFix(info));

    QDateTime t = parseGpsDateTimeLocalSafe(info.date, info.time);
    if (!t.isValid() && manual)
        t = QDateTime::currentDateTime();

    QString GPS_TimeStr = t.isValid() ? t.time().toString("HH:mm:ss") : info.time;
    QString GPS_DateStr = t.isValid() ? t.date().toString("yyyy-MM-dd") : info.date;

    double lat = info.lat;
    double lon = info.lon;
    double alt = info.alt;
    QString utmStr, mgrsStr;

    bool fallback = false;

    if (manual) {
        lat = m_manualGps1.lat;
        lon = m_manualGps1.lon;
        alt = m_manualGps1.alt;

        latLonToUTMAndMGRS(lat, lon, utmStr, mgrsStr, 5);

        m_lastGps1.valid = true;
        m_lastGps1.lat   = lat;
        m_lastGps1.lon   = lon;
        m_lastGps1.alt   = std::isfinite(alt) ? alt : 0.0;
        m_lastGps1.date  = GPS_DateStr;
        m_lastGps1.time  = GPS_TimeStr;
        m_lastGps1.utm   = utmStr;
        m_lastGps1.mgrs  = mgrsStr;

    } else if (good) {
        latLonToUTMAndMGRS(lat, lon, utmStr, mgrsStr, 5);

        m_lastGps1.valid = true;
        m_lastGps1.lat   = lat;
        m_lastGps1.lon   = lon;
        m_lastGps1.alt   = std::isfinite(alt) ? alt : 0.0;
        m_lastGps1.date  = GPS_DateStr;
        m_lastGps1.time  = GPS_TimeStr;
        m_lastGps1.utm   = utmStr;
        m_lastGps1.mgrs  = mgrsStr;

    } else if (m_lastGps1.valid) {
        fallback = true;
        lat      = m_lastGps1.lat;
        lon      = m_lastGps1.lon;
        alt      = m_lastGps1.alt;
        GPS_DateStr = m_lastGps1.date;
        GPS_TimeStr = m_lastGps1.time;
        utmStr   = m_lastGps1.utm;
        mgrsStr  = m_lastGps1.mgrs;
    } else {
        qDebug() << "[GPS1] no fix and no last -> skip marker";
        return;
    }

    const QString latStr = QString::number(lat, 'f', 6);
    const QString lonStr = QString::number(lon, 'f', 6);
    const QString altStr = QString::number(alt, 'f', 4);

    emit updateLocationLatLongFromGPS(latStr, lonStr, altStr, utmStr, mgrsStr);
    emit updatecurrentFromGPSTime(GPS_DateStr, GPS_TimeStr);
    emit updateGpsMarker(Serialnumber, controllerName, lat, lon, alt, GPS_DateStr, GPS_TimeStr);

    state1_ = info;

    if (chatServerDF) {
        QJsonObject obj;
        obj["menuID"]   = "UpdateGPSMarker";
        obj["source"]   = "GPS1";
        obj["locked"]   = manual ? false : (info.locked == 1);
        obj["satUse"]   = manual ? 0 : info.satUse;
        obj["manual"]   = manual;
        obj["fallback"] = fallback;
        obj["lat"]      = lat;
        obj["lon"]      = lon;
        obj["alt"]      = alt;
        obj["date"]     = GPS_DateStr;
        obj["time"]     = GPS_TimeStr;
        obj["utm"]      = utmStr;
        obj["mgrs"]     = mgrsStr;
        obj["raw_date"] = info.date;
        obj["raw_time"] = info.time;

        const QString jsonStr = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        chatServerDF->broadcastMessage(jsonStr);
        broadcastMessageServerandClient(obj);
    }
}

void iScreenDF::gps2Updated(const GPSInfo &info)
{
    const bool manual = m_manualGps2.enabled;
    const bool good = (!manual && isGoodFix(info));

    QDateTime t = parseGpsDateTimeLocalSafe(info.date, info.time);
    if (!t.isValid() && manual)
        t = QDateTime::currentDateTime();

    QString GPS_TimeStr = t.isValid() ? t.time().toString("HH:mm:ss") : info.time;
    QString GPS_DateStr = t.isValid() ? t.date().toString("yyyy-MM-dd") : info.date;

    double lat = info.lat;
    double lon = info.lon;
    double alt = info.alt;
    QString utmStr, mgrsStr;
    bool fallback = false;

    if (manual) {
        lat = m_manualGps2.lat;
        lon = m_manualGps2.lon;
        alt = m_manualGps2.alt;

        latLonToUTMAndMGRS(lat, lon, utmStr, mgrsStr, 5);

        m_lastGps2.valid = true;
        m_lastGps2.lat   = lat;
        m_lastGps2.lon   = lon;
        m_lastGps2.alt   = std::isfinite(alt) ? alt : 0.0;
        m_lastGps2.date  = GPS_DateStr;
        m_lastGps2.time  = GPS_TimeStr;
        m_lastGps2.utm   = utmStr;
        m_lastGps2.mgrs  = mgrsStr;

    } else if (good) {
        latLonToUTMAndMGRS(lat, lon, utmStr, mgrsStr, 5);

        m_lastGps2.valid = true;
        m_lastGps2.lat   = lat;
        m_lastGps2.lon   = lon;
        m_lastGps2.alt   = std::isfinite(alt) ? alt : 0.0;
        m_lastGps2.date  = GPS_DateStr;
        m_lastGps2.time  = GPS_TimeStr;
        m_lastGps2.utm   = utmStr;
        m_lastGps2.mgrs  = mgrsStr;

    } else if (m_lastGps2.valid) {
        fallback = true;
        lat      = m_lastGps2.lat;
        lon      = m_lastGps2.lon;
        alt      = m_lastGps2.alt;
        GPS_DateStr = m_lastGps2.date;
        GPS_TimeStr = m_lastGps2.time;
        utmStr   = m_lastGps2.utm;
        mgrsStr  = m_lastGps2.mgrs;
    } else {
        qDebug() << "[GPS2] no fix and no last -> skip marker";
        return;
    }

    const QString latStr = QString::number(lat, 'f', 6);
    const QString lonStr = QString::number(lon, 'f', 6);
    const QString altStr = QString::number(alt, 'f', 4);

    emit updateLocationLatLongFromGPS(latStr, lonStr, altStr, utmStr, mgrsStr);
    emit updatecurrentFromGPSTime(GPS_DateStr, GPS_TimeStr);

    state2_ = info;

    if (chatServerDF) {
        QJsonObject obj;
        obj["menuID"]   = "UpdateGPSMarker";
        obj["source"]   = "GPS2";
        obj["locked"]   = manual ? false : (info.locked == 1);
        obj["satUse"]   = manual ? 0 : info.satUse;
        obj["manual"]   = manual;
        obj["fallback"] = fallback;
        obj["lat"]      = lat;
        obj["lon"]      = lon;
        obj["alt"]      = alt;
        obj["date"]     = GPS_DateStr;
        obj["time"]     = GPS_TimeStr;
        obj["utm"]      = utmStr;
        obj["mgrs"]     = mgrsStr;
        obj["raw_date"] = info.date;
        obj["raw_time"] = info.time;

        const QString jsonStr = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        chatServerDF->broadcastMessage(jsonStr);
        broadcastMessageServerandClient(obj);
    }
}
/////////////////////////////GPSInfo END//////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// \brief Mainwindows::gpioInit
///
void iScreenDF::gpioInit()
{
#ifdef PLATFORM_JETSON
    displaysetting = new newGPIOClassDF(GPIO_SETUP_DISPLAY);

    displaysetting->requestOutput();
    QThread::msleep(200);
    displaysetting->setValue(true);
// QThread::msleep(200);
// qDebug() << "GPIO_SETUP_DISPLAY";
// displaysetting->setValue(false);
// QThread::msleep(200);
// displaysetting->setValue(true);
#endif
}


float iScreenDF::getMemUsage() {
    QFile file("/proc/meminfo");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0.0;
    }

    QTextStream in(&file);
    QString memInfo;
    int totalMem = 0;
    int freeMem = 0;

    while (in.readLineInto(&memInfo)) {
        QStringList fields = memInfo.split(QRegExp("\\s+"));

        if (fields.size() >= 2) {
            if (fields.at(0) == "MemTotal:") {
                totalMem = fields.at(1).toInt();
            }
            if (fields.at(0) == "MemFree:") {
                freeMem = fields.at(1).toInt();
            }
        }
        if ((totalMem != 0) && (freeMem != 0))
            break;

        // qDebug() << "Line read from /proc/meminfo: " << memInfo;
    }
    int usedMem = totalMem - freeMem;

    if (totalMem > 0) {
        float usagePercentage = float(usedMem * 100.0) / totalMem;

        // qDebug() << "totalMem" << totalMem  << "MemFree" << freeMem << "usedMem" << usedMem << "Memory usage: " << usagePercentage << "%";

        return usagePercentage;
    }

    return 0.0;
}

void * iScreenDF::ThreadFunc(void * pTr) {
    iScreenDF * pThis = static_cast < iScreenDF * > (pTr);
    qDebug() << "ThreadFunc1";
    pThis -> loopGetInfo();
    return NULL;
}

void iScreenDF::loopGetInfo() {
    while (1) {
        QString currentTime = QTime::currentTime().toString("hh:mm:ss");
        if (currentTime != lastGetCurrentTime) {
            lastGetCurrentTime = currentTime;
            QString currentTime = QTime::currentTime().toString("hh:mm:ss");
            QString currentDate = QDate::currentDate().toString("dd MMM yyyy");
            QString uptimeStr = getUPTime().replace("up", "");
            QString message = QString("{\"menuID\":\"broadcastLocalTime\", \"currentTime\":\"%1\", \"currentDate\":\"%2\", \"uptime\":\"%3\"}")
                                  .arg(currentTime)
                                  .arg(currentDate)
                                  .arg(uptimeStr);
            sendToWeb(message);
        }
        if (count == 15){
            memUsage = getMemUsage();
            count = 0;
            // qDebug()<<"memUsage"<<memUsage;
            // if (memUsage == 80){
            // SET_AND_EMIT(m_memUsage,memUsage, memUsageChanged);
            // }
            if (memUsage >= 90){
                system("systemctl restart iScreenKraken.service");
                // exit(1);
            }
        }
        count ++;
        // qDebug()<<"count"<<count;
        QThread::msleep(500);
    }
}

QString iScreenDF::getUPTime() {
    system("uptime -p > /etc/uptime");
    QString fileName = QString("/etc/uptime");
    return readLine(fileName);
}

QString iScreenDF::readLine(const QString &fileName) {
    QFile inputFile(fileName);
    inputFile.open(QIODevice::ReadOnly);
    if (!inputFile.isOpen()) return "";

    QTextStream stream(&inputFile);
    QString line = stream.readLine();
    inputFile.close();
    return line.trimmed();
}

void iScreenDF::socketClientClosed(int socketID, const QString &ip)
{
    qDebug() << "[Mainwindows::socketClientClosed]"
             << "socketID:" << socketID
             << "ip:" << ip;

    for (groupActive *grp : group_active_list) {
        if (!grp) continue;

        for (clientNode *node : grp->client_active_list) {
            if (!node || !node->chatclient)
                continue;

            // ใช้ socketID จาก ChatClient และ IP
            if (node->chatclient->m_socketID == socketID &&
                node->ipAddress == ip)
            {
                qDebug() << "  -> mark client disconnected for deviceID"
                         << node->deviceID << "ip:" << node->ipAddress;

                node->Connected    = false;
                node->status       = 0;
                node->descriptions = "Disconnected";

                return;
            }
        }
    }

    qWarning() << "[socketClientClosed] No matching clientNode for socketID"
               << socketID << "ip" << ip;
}

void iScreenDF::applyManualGps(double lat, double lon, double alt)
{
    if (!std::isfinite(lat) || !std::isfinite(lon) || !std::isfinite(alt)) {
        qWarning() << "[ManualGPS] invalid input:" << lat << lon << alt;
        return;
    }

    m_manualGps1.enabled = true;
    m_manualGps1.lat = lat;
    m_manualGps1.lon = lon;
    m_manualGps1.alt = alt;

    m_manualGps2.enabled = true;
    m_manualGps2.lat = lat;
    m_manualGps2.lon = lon;
    m_manualGps2.alt = alt;

    QString utmStr, mgrsStr;
    latLonToUTMAndMGRS(lat, lon, utmStr, mgrsStr, 5);

    const QString dateStr = QDate::currentDate().toString("yyyy-MM-dd");
    const QString timeStr = QTime::currentTime().toString("HH:mm:ss");

    m_lastGps1.valid = true;
    m_lastGps1.lat   = lat;
    m_lastGps1.lon   = lon;
    m_lastGps1.alt   = alt;
    m_lastGps1.date  = dateStr;
    m_lastGps1.time  = timeStr;
    m_lastGps1.utm   = utmStr;
    m_lastGps1.mgrs  = mgrsStr;

    m_lastGps2.valid = true;
    m_lastGps2.lat   = lat;
    m_lastGps2.lon   = lon;
    m_lastGps2.alt   = alt;
    m_lastGps2.date  = dateStr;
    m_lastGps2.time  = timeStr;
    m_lastGps2.utm   = utmStr;
    m_lastGps2.mgrs  = mgrsStr;

    emit updateLocationLatLongFromGPS(
        QString::number(lat, 'f', 6),
        QString::number(lon, 'f', 6),
        QString::number(alt, 'f', 4),
        utmStr,
        mgrsStr
        );

    emit updatecurrentFromGPSTime(dateStr, timeStr);
    emit updateGpsMarker(Serialnumber, controllerName, lat, lon, alt, dateStr, timeStr);

    if (chatServerDF) {
        QJsonObject obj1;
        obj1["menuID"]   = "UpdateGPSMarker";
        obj1["source"]   = "GPS1";
        obj1["locked"]   = false;
        obj1["satUse"]   = 0;
        obj1["manual"]   = true;
        obj1["fallback"] = false;
        obj1["lat"]      = lat;
        obj1["lon"]      = lon;
        obj1["alt"]      = alt;
        obj1["date"]     = dateStr;
        obj1["time"]     = timeStr;
        obj1["utm"]      = utmStr;
        obj1["mgrs"]     = mgrsStr;
        obj1["raw_date"] = "";
        obj1["raw_time"] = "";

        const QString jsonStr1 = QString::fromUtf8(QJsonDocument(obj1).toJson(QJsonDocument::Compact));
        chatServerDF->broadcastMessage(jsonStr1);
        broadcastMessageServerandClient(obj1);

        QJsonObject obj2 = obj1;
        obj2["source"] = "GPS2";

        const QString jsonStr2 = QString::fromUtf8(QJsonDocument(obj2).toJson(QJsonDocument::Compact));
        chatServerDF->broadcastMessage(jsonStr2);
        broadcastMessageServerandClient(obj2);
    }

    qDebug() << "[ManualGPS] applyManualGps =" << lat << lon << alt;
}

void iScreenDF::disableManualGps1()
{
    m_manualGps1.enabled = false;
    qDebug() << "[ManualGPS] GPS1 manual disabled";
}

void iScreenDF::disableManualGps2()
{
    m_manualGps2.enabled = false;
    qDebug() << "[ManualGPS] GPS2 manual disabled";
}
