#include "mainwindowsiRec.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
//OTL324$
mainwindowsiRec::mainwindowsiRec(QString platform,QObject *parent) : QObject(parent)
{
    qDebug() << "[mainwindows] starting ChatServer on port" << port;
    SocketServer = new ChatServeriRec(port, this);   // << ใส่ parent กันลืม
    m_webServer = new ChatServerWebRec(1235, this);
    //    mysql        = new DatabaseiRec("recorder", "iScreenKraken", "OTL324$", "localhost", this);
    mysql        = new DatabaseiRec("recorder", "iScreenKraken", "Ifz8zean6868**", "localhost", this);
    max31760     = new MAX31760(this);
    max9850      = new Max9850("7", 0x10);
    UnixSocketListener* unixReceiver = new UnixSocketListener(this);
    m_unixReceiver = new UnixSocketListener(this);
    dataLoggerServer = new ChatClientiGate(8088,"127.0.0.1","",1,enableDataLogger);
    dataLoggerServer->setDevice("Server","localhost");

    //--------------------------------------mainwindows------------------------------------------------//
    connect(m_unixReceiver, &UnixSocketListener::messageReceived,this, &mainwindowsiRec::onUnixSocketMessage);
    connect(m_webServer,&ChatServerWebRec::cppCommandToWeb,this, &mainwindowsiRec::cppSubmitTextFiled);
    connect(mysql, SIGNAL(commandMysqlToCpp(QString)), this, SLOT(cppSubmitTextFiledMySQL(QString)));
    connect(mysql, SIGNAL(previousRecordVolume( QString)), this, SLOT(cppSubmitTextFiledMySQL( QString)));
    connect(mysql, SIGNAL(commandMysqlToWeb( QString)), m_webServer, SLOT(broadcastMessage( QString)));
    connect(mysql, &DatabaseiRec::commandMysqlToWeb,m_webServer, &ChatServerWebRec::broadcastMessage);
    connect(mysql, &DatabaseiRec::verifyUserDatabaseDone,this, &mainwindowsiRec::onVerifyUserDatabaseDone);

    int ret = pthread_create(&idThreaddatetime, nullptr, ThreadFuncDateTime, this);
    if (ret == 0) qDebug() << "Thread created successfully.";
    else          qDebug() << "Thread not created.";

    ret=pthread_create(&idThreadFan, NULL, ThreadFuncFan, this);
    if(ret==0){
        qDebug() <<("Thread created successfully.\n");
    }
    else{
        qDebug() <<("Thread not created.\n");
    }
    ret=pthread_create(&idThread, NULL, ThreadFunc, this);
    if(ret==0){
        qDebug() <<("Thread created successfully.\n");
    }
    else{
        qDebug() <<("Thread not created.\n");
    }

    mysql->VerifyUserDatabase();
    QTimer::singleShot(1000, this, [this]() {
        qDebug() << "[Audio] Enable I2S Loopback";
        enableI2SLoopback();
        ensureVoicexSymlinkAndFix();
    });
    VerifyFolderAndText();
}

mainwindowsiRec::~mainwindowsiRec()
{
    m_threadRunning = false;
    void* dummy = nullptr;
    pthread_join(idThreaddatetime, &dummy);
}

void mainwindowsiRec::VerifyFolderAndText()
{
    const QString basePath = "/home/orinnx/saveFileName";
    const QString filePath = basePath + "/filesNameWave.txt";

    // ===== เช็คโฟลเดอร์ =====
    QDir dir(basePath);
    if (!dir.exists()) {
        QDir home("/home/orinnx");
        home.mkpath("saveFileName");   // mkpath ปลอดภัย มีอยู่แล้วก็ไม่ error
    }

    // ===== เช็คไฟล์ =====
    QFileInfo fi(filePath);
    if (fi.exists() && fi.isFile()) {
        return;   // มีครบแล้ว ออกเลย
    }

    // ===== สร้างไฟล์ + เนื้อหาเริ่มต้น =====
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }

    QTextStream out(&file);
    out << "#FILES\n";
    out << "#SUMMARY totalFiles=0 sampleCount=0 totalMs=0 totalDurationSec=0.000 samplesLength=0 totalSizeKB=0.000\n";
    file.close();
}
void mainwindowsiRec::onVerifyUserDatabaseDone(bool ok, const QString& message)
{
    qDebug() << "[VerifyUserDatabaseDone] ok=" << ok << "message=" << message;
    if (!ok) {
        return;
    }
    system("systemctl daemon-reload");
    RestartSystemServicesAfter30s();
}
void mainwindowsiRec::RestartSystemServicesAfter30s(){
    qDebug() << "<<<<<<<---Restart service system--->>>>>>>";
    system("systemctl reset-failed alsarecd.service");
    system("systemctl stop alsarecd.service");
    QTimer::singleShot(30000, this, [this]() {
        system("systemctl restart irecd.service");
        system("systemctl restart iplayd.service");
        QTimer::singleShot(15000, this, [this]() {
            system("systemctl restart iGateRec@1.service");
            QTimer::singleShot(30000, this, [this]() {
                qDebug() << "<<<<<<<---Restart service system alsarecd.service--->>>>>>>";
                system("systemctl restart alsarecd.service");
            });
        });
    });
}
void mainwindowsiRec::enableI2SLoopback()
{
    const QString checkCmd =
        "amixer -c APE sget 'I2S1 Loopback' | grep -qi '\\bon\\b'";

    {
        QProcess check;
        check.start("/bin/bash", QStringList() << "-lc" << checkCmd);
        check.waitForFinished(2000);

        if (check.exitCode() == 0) {
            qDebug() << "[I2S Loopback] already ON → skip enable";
            return;
        }
    }

    qDebug() << "[I2S Loopback] enabling all I2S loopbacks";

    static const QStringList cmds = {
        "amixer -c APE sset 'I2S1 Loopback' on",
        "amixer -c APE sset 'I2S2 Loopback' on",
        "amixer -c APE sset 'I2S3 Loopback' on",
        "amixer -c APE sset 'I2S4 Loopback' on"
    };

    for (const QString &cmd : cmds) {
        QProcess p;
        p.start("/bin/bash", QStringList() << "-lc" << cmd);

        if (!p.waitForFinished(3000)) {
            qWarning() << "[I2S Loopback] timeout:" << cmd;
            continue;
        }

        const QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();

        if (!out.isEmpty())
            qDebug() << "[I2S Loopback OUT]" << out;
        if (!err.isEmpty())
            qWarning() << "[I2S Loopback ERR]" << err;
    }
}
void mainwindowsiRec::ensureVoicexSymlinkAndFix()
{
    const QString realDir = "/var/ivoicex";
    const QString linkDir = "/var/voicex";

    auto canonicalOrAbs = [](const QString &path) -> QString {
        QFileInfo fi(path);
        const QString canon = fi.canonicalFilePath();
        return canon.isEmpty() ? fi.absoluteFilePath() : canon;
    };

    auto hasIvoicexLoop = [&](const QString &path) -> bool {
        QFileInfo fi(path);
        if (!fi.exists())
            return false;

        const QString p = canonicalOrAbs(path);
        // loop: /ivoicex ซ้ำ >= 2 เช่น /var/ivoicex/ivoicex/ivoicex
        return (p.count("/ivoicex") >= 2);
    };

    auto removeBad = [&]() {
        QProcess p;
        p.start("/bin/bash", QStringList() << "-lc" << "rm -rf /var/ivoicex /var/voicex");
        p.waitForFinished(30000);

        const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
        if (!err.isEmpty())
            qWarning() << "[voicex] rm err:" << err;
    };

    auto ensureRealDir = [&]() -> bool {
        if (QFileInfo::exists(realDir)) {
            if (!QFileInfo(realDir).isDir()) {
                qWarning() << "[voicex] /var/ivoicex exists but not a directory -> remove";
                return false;
            }
            return true;
        }

        if (!QDir().mkpath(realDir)) {
            qCritical() << "[voicex] failed to create directory:" << realDir;
            return false;
        }
        qDebug() << "[voicex] created directory:" << realDir;
        return true;
    };

    auto ensureSymlink = [&]() -> bool {
        QFileInfo linkInfo(linkDir);

        if (linkInfo.exists() || linkInfo.isSymLink()) {
            if (linkInfo.isSymLink()) {
                const QString target = linkInfo.symLinkTarget();
                const QString targetCanon = canonicalOrAbs(target);
                const QString realCanon   = canonicalOrAbs(realDir);

                if (targetCanon == realCanon) {
                    qDebug() << "[voicex] symlink already OK:" << linkDir << "->" << target;
                    return true;
                }

                qWarning() << "[voicex] symlink exists but wrong target:" << linkDir << "->" << target;
            } else {
                qWarning() << "[voicex] /var/voicex exists but not a symlink -> remove";
            }

            QProcess p;
            p.start("/bin/bash", QStringList() << "-lc" << "rm -rf /var/voicex");
            p.waitForFinished(30000);
        }

        if (!QFile::link(realDir, linkDir)) {
            qCritical() << "[voicex] failed to create symlink:" << linkDir << "->" << realDir;
            return false;
        }

        qDebug() << "[voicex] symlink created:" << linkDir << "->" << realDir;
        return true;
    };

    // =========================
    // STEP 0: ถ้าไม่เจอ /var/ivoicex เลย -> สร้าง + link
    // =========================
    if (!QFileInfo::exists(realDir)) {
        qWarning() << "[voicex] /var/ivoicex not found -> create + link";
        if (!ensureRealDir())
            return;
        if (!ensureSymlink())
            return;

        RestartSystemServicesAfter30s();
        return;
    }

    // =========================
    // STEP 1: ถ้าเจอ loop -> ลบทั้งหมด แล้วสร้างใหม่ + link
    // =========================
    if (hasIvoicexLoop(realDir)) {
        qWarning() << "[voicex] loop detected -> remove ivoicex/voicex and recreate";
        removeBad();

        if (!ensureRealDir())
            return;
        if (!ensureSymlink())
            return;

        RestartSystemServicesAfter30s();
        return;
    }

    // =========================
    // STEP 2: ปกติ ไม่ loop -> ensure symlink
    // =========================
    if (!ensureRealDir())
        return;

    if (!ensureSymlink())
        return;

    RestartSystemServicesAfter30s();
}

void mainwindowsiRec::getDateTime()
{
    // Update current date/time for display
    QDateTime currentDateTime = QDateTime::currentDateTime();

    const QDate d = currentDateTime.date();
    date = QString::number(d.year()) + '/'
           + QString::number(d.month()).rightJustified(2, '0') + '/'
           + QString::number(d.day()).rightJustified(2, '0');

    const QTime t = currentDateTime.time();
    time = QString::number(t.hour()).rightJustified(2, '0') + ':'
           + QString::number(t.minute()).rightJustified(2, '0') + ':'
           + QString::number(t.second()).rightJustified(2, '0');

    //    qDebug() << "date:" << date << "time:" << time;

    static int lastRunHour = -1;
    QTime nowTime = QTime::currentTime();

    int targetMinute = 0;
    int targetSecond = 0;

    if (nowTime.minute() == targetMinute && nowTime.second() == targetSecond && nowTime.hour() != lastRunHour) {
        int ret = pthread_create(&idThread4, nullptr, ThreadFunc4, this);
        if (ret == 0) {
            qDebug() << QString("[Hourly %1:00] Thread4 created successfully.").arg(nowTime.hour());
            lastRunHour = nowTime.hour();
        } else {
            qWarning() << QString("[Hourly %1:00] Thread4 not created.").arg(nowTime.hour());
        }
    }

}

void mainwindowsiRec::startRuntime()
{
    qInfo() << "[mainwindows] startRuntime() -> QML connected";
    m_qmlConnected = true;

    // m_clock.setInterval(1000);
    // m_clock.setTimerType(Qt::CoarseTimer);
    // connect(&m_clock, &QTimer::timeout, this, &mainwindowsiRec::calendar);
    // m_clock.start();
}


void mainwindowsiRec::cppSubmitTextFiled(QString qmlJson)
{
    qDebug() << "hello world" << qmlJson;
    QJsonDocument d = QJsonDocument::fromJson(qmlJson.toUtf8());
    QJsonObject command = d.object();
    QString getCommand =  QJsonValue(command["objectName"]).toString().trimmed();
    QString menuID     =  QJsonValue(command["menuID"]).toString().trimmed();
    QString getCommand2=  QJsonValue(command["objectNames"]).toString();
    QString getEventAndAlert = QJsonValue(command["TrapsAlert"]).toString();

    QByteArray br = qmlJson.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);
    QJsonObject obj = doc.object();

    getCommand = getCommand.trimmed();
    QWebSocket* wClient;
    clientSocket = wClient;
    if (getCommand ==  "socketConnect"){
        QString socketConnect = QJsonValue(command["socketCPP"]).toString();
        qDebug() << "socketConnect:" << socketConnect;

        if (socketConnect == "true"){
            QJsonObject obj;
            obj["objectName"] = "socketPort";
            obj["port"] = port;
            QJsonDocument doc(obj);
            QString socketPort = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
            cppCommand(socketPort);

            InitializingRTCtoSystem();
            mysql->updateRecordVolume();
        }
    }else if(menuID ==  "getRecordFiles"){
        qDebug() << "getRecordFiles:" << qmlJson;
        mysql->fetchAllRecordFiles(qmlJson,clientSocket);

    }else if (obj["menuID"].toString() == "ChangeNextPageOfRecord") {
        qDebug() << "ChangeNextPageOfRecord:" << qmlJson;
        mysql->nextPageOfRecorderFiles(qmlJson, wClient);
    }else if (obj["menuID"].toString() == "searchRecordFiles"
               || obj["menuID"].toString() == "searchRecordFilesWeb")
    {
        const QString menuID    = obj["menuID"].toString();
        qDebug() << "searchRecordFiles / Web:" << menuID << qmlJson;

        // ดึงค่าเหมือนเดิม
        QString startDate = obj["startDate"].toString();
        QString endDate   = obj["endDate"].toString();
        QString device    = obj["device"].toString();
        QString interval  = obj["interval"].toString();
        QString frequency = obj["frequency"].toString();
        const QString objectName =
            (menuID == "searchRecordFilesWeb")
                ? "searchRecordFilesWeb"
                : "searchRecordFiles";

        QString updatesearchRecordFiles = QString(
                                              "{"
                                              "\"objectName\":\"%1\","
                                              "\"startDate\":\"%2\","
                                              "\"endDate\":\"%3\","
                                              "\"device\":\"%4\","
                                              "\"interval\":\"%5\","
                                              "\"frequency\":\"%6\""
                                              "}"
                                              )
                                              .arg(objectName)
                                              .arg(startDate)
                                              .arg(endDate)
                                              .arg(device)
                                              .arg(interval)
                                              .arg(frequency);

        qDebug() << "[cppSubmitTextFiledMySQL] updatesearchRecordFiles:"
                 << updatesearchRecordFiles;

        mysql->searchRecordFilesMysql(updatesearchRecordFiles, wClient);
    }else if (obj["menuID"].toString() == "getRegisterDevicePage"
               || obj["menuID"].toString() == "getRegisterDevicePageWeb") {
        const QString menuID = obj["menuID"].toString();
        qDebug() << "getRegisterDevicePage / Web:" << menuID << qmlJson;
        mysql->cancelFetchRecordFiles = true;
        mysql->getRegisterDevicePage(qmlJson, wClient);
    }else if (obj["menuID"].toString() == "updateDevice" || obj["menuID"].toString() == "updateDeviceWeb") {
        qDebug() << "updateDevice:" << qmlJson;
        mysql->CheckAndHandleDevice(qmlJson,wClient);
    }else if (obj["menuID"].toString() == "RegisterDevice" || obj["menuID"].toString() == "RegisterDeviceWeb") {
        qDebug() << "RegisterDevice:" << qmlJson;
        QJsonDocument doc = QJsonDocument::fromJson(qmlJson.toUtf8());
        QJsonObject o = doc.object();
        if (!o.contains("storage_path") && !o.contains("file_path")) {
            o["storage_path"] = "/var/ivoicex";
        }
        const QString updatedJson = QString::fromUtf8(
            QJsonDocument(o).toJson(QJsonDocument::Compact)
            );
        qDebug() << "RegisterDevice (patched) ->" << updatedJson;
        mysql->CheckAndHandleDevice(updatedJson, wClient);

    }else if (obj["menuID"].toString() == "deleteDevice" || obj["menuID"].toString() == "deleteDeviceWeb") {
        qDebug() << "deleteDeviceWeb ->" << qmlJson;
        mysql->removeRegisterDevice(qmlJson,wClient);
    }else if (getCommand ==  "Screenshot"){
        qDebug() << "captureScreenshot:" << qmlJson;

        emit captureScreenshot();
    }else if (obj["menuID"].toString() == "scanDeivce") {

        // 1) เริ่มต้นแจ้งว่า scanning...
        {
            QJsonObject st;
            st["menuID"] = "statusScanDevice";
            st["status"] = "Scanning...";
            cppCommand(QJsonDocument(st).toJson(QJsonDocument::Compact));
        }

        qDebug() << "scanDeivce:" << qmlJson;

        // 2) Scan รอบแรก
        QJsonArray devs = scanSdDevices();

        if (devs.isEmpty()) {
            QJsonObject st;
            st["menuID"] = "statusScanDevice";
            st["status"] = "Not Found Device";
            cppCommand(QJsonDocument(st).toJson(QJsonDocument::Compact));
        } else {
            QJsonObject st;
            st["menuID"] = "statusScanDevice";
            st["status"] = QString("Found %1 Device").arg(devs.size());
            cppCommand(QJsonDocument(st).toJson(QJsonDocument::Compact));
        }

        // 3) Auto-mount เฉพาะ partition sda1/sdb1...
        for (int i = 0; i < devs.size(); ++i) {
            if (!devs.at(i).isObject())
                continue;
            QJsonObject d = devs.at(i).toObject();

            QString name = d.value("name").toString();  // "sda1"
            QString type = d.value("type").toString();  // "part"
            bool mounted = d.value("mounted").toBool();

            if (type != "part") continue;
            if (!name.startsWith("sd") || name.size() < 4) continue;

            QChar c = name.at(2);
            if (c < QChar('a') || c > QChar('z')) continue;
            if (mounted) continue;

            if (!mountSdDevice(name)) {
                qWarning() << "[scanDeivce] mountSdDevice failed for" << name;
            }
        }

        // 4) scan อีกครั้งหลัง mount
        devs = scanSdDevices();

        {
            QJsonObject st;
            st["menuID"] = "statusScanDevice";
            st["status"] = QString("Found %1 Device").arg(devs.size());
            cppCommand(QJsonDocument(st).toJson(QJsonDocument::Compact));
        }

        // 5) ส่งผลลัพธ์หลัก
        QJsonObject resp;
        resp["menuID"]  = "scanDeivceResult";
        resp["ok"]      = true;
        resp["devices"] = devs;

        cppCommand(QJsonDocument(resp).toJson(QJsonDocument::Compact));

        // 6) ส่ง Done
        {
            QJsonObject st;
            st["menuID"] = "statusScanDevice";
            st["status"] = "Done";
            cppCommand(QJsonDocument(st).toJson(QJsonDocument::Compact));
        }
    }else if (obj["menuID"].toString() == "unmountDeivce") {
        qDebug() << "unmountDeivce:" << qmlJson;

        QString devName = obj.value("devName").toString().trimmed();

        bool ok = false;

        if (!devName.isEmpty()) {
            // เคสที่ QML ระบุชื่อ device มา เช่น "sda1"
            ok = unmountSdDevice(devName);
        } else {
            // เคสที่ QML ไม่ได้บอกว่าอะไร → unmount ทุก /media/usb_sd*
            ok = unmountAllUsbSd();
        }

        QJsonObject resp;
        resp["menuID"]  = "unmountDeivceResult";
        resp["ok"]      = ok;
        resp["devName"] = devName;   // ถ้าว่าง แปลว่าเป็น all

        QString out = QJsonDocument(resp).toJson(QJsonDocument::Compact);
        qDebug() << "[unmountDeivce] result:" << out;
        cppCommand(out);
    }else if (menuID == "exportMergeFilesToUSB") {
        qDebug() << "[exportMergeFilesToUSB] payload:" << qmlJson;

        const QString mountPoint = obj.value("mountPoint").toString();
        QString fileName        = obj.value("fileName").toString();
        QJsonArray filesArr     = obj.value("files").toArray();

        // เตรียม response ไว้ส่งกลับ QML
        QJsonObject resp;
        resp["menuID"] = "exportMergeFilesToUSBResult";

        // เช็ค input เบื้องต้น
        if (filesArr.isEmpty()) {
            qWarning() << "[exportMergeFilesToUSB] no files";
            resp["ok"]    = false;
            resp["error"] = "no_files";
            QString out = QJsonDocument(resp).toJson(QJsonDocument::Compact);
            cppCommand(out);
            return;
        }
        if (mountPoint.isEmpty()) {
            qWarning() << "[exportMergeFilesToUSB] mountPoint is empty";
            resp["ok"]    = false;
            resp["error"] = "mount_point_empty";
            QString out = QJsonDocument(resp).toJson(QJsonDocument::Compact);
            cppCommand(out);
            return;
        }

        // เตรียม path ปลายทาง
        QString outDir = mountPoint;
        if (!outDir.endsWith('/'))
            outDir += '/';

        if (fileName.isEmpty())
            fileName = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");

        // ดึง list ไฟล์ input
        // ดึง list ไฟล์ input
        QStringList inputFiles;
        for (const QJsonValue &v : filesArr) {
            QString p;
            if (v.isObject()) {
                QJsonObject fo = v.toObject();
                p = fo.value("full_path").toString();
                if (p.isEmpty())
                    p = fo.value("fullPath").toString();
            }
            else if (v.isString()) {
                p = v.toString();
            }
            if (!p.isEmpty())
                inputFiles << p;
        }


        if (inputFiles.isEmpty()) {
            qWarning() << "[exportMergeFilesToUSB] inputFiles empty after parsing";
            resp["ok"]    = false;
            resp["error"] = "no_valid_paths";
            QString out = QJsonDocument(resp).toJson(QJsonDocument::Compact);
            cppCommand(out);
            return;
        }

        qDebug() << "[exportMergeFilesToUSB] outDir =" << outDir;
        qDebug() << "[exportMergeFilesToUSB] baseName =" << fileName;
        qDebug() << "[exportMergeFilesToUSB] inputs =" << inputFiles;

        QString outFinalPath;
        QString errorReason;

        bool ok = mergeAndConvertToMp3(inputFiles, outDir, fileName, outFinalPath, errorReason);

        if (!ok) {
            resp["ok"]    = false;
            resp["error"] = errorReason.isEmpty() ? "merge_convert_failed" : errorReason;
            qWarning() << "[exportMergeFilesToUSB] failed:" << resp["error"].toString();
            emit exportFinished(false, QString(), resp["error"].toString());
        } else {
            resp["ok"]      = true;
            resp["outPath"] = outFinalPath;
            qDebug() << "[exportMergeFilesToUSB] success mp3:" << outFinalPath;
            // exportFinished(true, ...) ถูก emit ใน mergeAndConvertToMp3 แล้ว
        }


        QString out = QJsonDocument(resp).toJson(QJsonDocument::Compact);
        cppCommand(out);
    }else if (menuID == "setVolume") {
        qDebug() << "setVolume:" << qmlJson;

        double volume = obj["currentVolume"].toDouble();   // 0.0–1.0
        convertToPercent = volume * 100.0;          // 0–100

        level = 0;   // default เผื่อหลุดช่วง

        if (convertToPercent >= 0 && convertToPercent <= 10) {
            level = 1;
            qDebug() << "0-10%  -> level" << level << "value:" << convertToPercent;
        } else if (convertToPercent > 10 && convertToPercent <= 20) {
            level = 2;
            qDebug() << "11-20% -> level" << level << "value:" << convertToPercent;
        } else if (convertToPercent > 20 && convertToPercent <= 30) {
            level = 3;
            qDebug() << "21-30% -> level" << level << "value:" << convertToPercent;
        } else if (convertToPercent > 30 && convertToPercent <= 40) {
            level = 4;
            qDebug() << "31-40% -> level" << level << "value:" << convertToPercent;
        } else if (convertToPercent > 40 && convertToPercent <= 50) {
            level = 5;
            qDebug() << "41-50% -> level" << level << "value:" << convertToPercent;
        } else if (convertToPercent > 50 && convertToPercent <= 60) {
            level = 6;
            qDebug() << "51-60% -> level" << level << "value:" << convertToPercent;
        } else if (convertToPercent > 60 && convertToPercent <= 70) {
            level = 7;
            qDebug() << "61-70% -> level" << level << "value:" << convertToPercent;
        } else if (convertToPercent > 70 && convertToPercent <= 80) {
            level = 8;
            qDebug() << "71-80% -> level" << level << "value:" << convertToPercent;
        } else if (convertToPercent > 80 && convertToPercent <= 90) {
            level = 9;
            qDebug() << "81-90% -> level" << level << "value:" << convertToPercent;
        } else if (convertToPercent > 90) {
            level = 10;
            qDebug() << "91-100% -> level" << level << "value:" << convertToPercent;
        }
        mysql->recordVolume(convertToPercent, level);
        currentVolume = qBound(0, currentVolume, 63);
        qDebug() << "Adjusted currentVolume:" << currentVolume;

        if (!max9850->setVolume(currentVolume)) {
            qWarning() << "Failed to set volume:" << currentVolume;
            return;
        }
    }else if (menuID == "refreshpage") {
        mysql->fetchAllRecordFiles(qmlJson,clientSocket);
    }else if (menuID == "getSystemPageWeb") {
        qDebug() << "getSystemPageWeb:";
    }else if (obj["menuID"].toString() == "deletedFileWave") {
        qDebug() << "deletedFileWave:";
        mysql->deletedFileWave(qmlJson, wClient);
    }else {
        qWarning() << "[cppSubmitTextFiledMySQL] unknown menuID:" << menuID << qmlJson;
    }
}




void mainwindowsiRec::cppSubmitTextFiledMySQL(QString qmlJson){
    qDebug() << "cppSubmitTextFiledMySQL" << qmlJson;
    QJsonDocument d = QJsonDocument::fromJson(qmlJson.toUtf8());
    QJsonObject command = d.object();
    QString getCommand =  QJsonValue(command["objectName"]).toString().trimmed();
    QString menuID     =  QJsonValue(command["menuID"]).toString().trimmed();
    QString getCommand2=  QJsonValue(command["objectNames"]).toString();
    QString getEventAndAlert = QJsonValue(command["TrapsAlert"]).toString();
    QByteArray br = qmlJson.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);
    QJsonObject obj = doc.object();

    getCommand = getCommand.trimmed();
    if (getCommand ==  "recordFilesChunk"){
        qDebug() << "recordFilesChunk:" << qmlJson;
        cppCommand(qmlJson);
    }else if (obj["menuID"].toString() == "deviceList"){
        qDebug() << "deviceList:" << qmlJson;
        cppCommand(qmlJson);
    }else if (obj["menuID"].toString() == "statusSearchFiles"){
        qDebug() << "statusSearchFiles:" << qmlJson;
        cppCommand(qmlJson);
    }else if (obj["menuID"].toString() == "updateRecordVolume"){
        qDebug() << "updateRecordVolume:" << qmlJson;
        cppCommand(qmlJson);
    }else if (obj["menuID"].toString() == "recordFilesUpdate"){
        qDebug() << "recordFilesUpdate:" << qmlJson;
        cppCommand(qmlJson);
    }else if (obj["menuID"].toString() == "updateDevice"){
        qDebug() << "updateDevice:" << qmlJson;
        cppCommand(qmlJson);
    }
}


void mainwindowsiRec::checkAndUpdateRTC()
{
    QProcess ntpCheck;
    ntpCheck.start("timedatectl show -p NTPSynchronized --value");
    ntpCheck.waitForFinished(1000);
    QString ntpStatus = ntpCheck.readAllStandardOutput().trimmed();

    if (ntpStatus == "yes") {
        qDebug() << "NTP is active → Updating hwclock from system time";
        system("hwclock -w");
    } else {
        qDebug() << "NTP not active → Will retry in 15 minutes";
    }
}
void mainwindowsiRec::InitializingRTCtoSystem()
{
    qDebug() << "[InitializingRTCtoSystem]";

    static bool initialized = false;
    if (!initialized) {
        qDebug() << "Initializing: set system time from RTC";
        system("hwclock -s");

        // เริ่ม QTimer เพื่อเช็กทุก 15 นาที
        rtcUpdateTimer = new QTimer(this);
        connect(rtcUpdateTimer, &QTimer::timeout, this, &mainwindowsiRec::checkAndUpdateRTC);
        rtcUpdateTimer->start(15 * 60 * 1000);  // 15 นาที
        checkAndUpdateRTC();  // เรียกทันทีตอนเริ่มแรก
        initialized = true;
    }
    //    QTimer::singleShot(15000, this, [=]() {
    //        qDebug() << "Restarting irecd.service after 20s delay...";
    //        system("systemctl restart irecd.service");
    //        system("systemctl restart iplayd.service");
    //    });
}


// ===== helper แปลง size จาก lsblk =====
static qulonglong parseLsblkSize(const QJsonValue &val)
{
    if (val.isDouble()) {
        double d = val.toDouble();
        if (d < 0) d = 0;
        return static_cast<qulonglong>(d);
    }

    QString s = val.toString().trimmed();
    if (s.isEmpty())
        return 0;

    bool ok = false;
    qulonglong n = s.toULongLong(&ok);
    if (ok)
        return n;

    if (s.size() < 2)
        return 0;

    QChar suffix = s.at(s.size() - 1).toUpper();
    s.chop(1);

    double base = s.toDouble(&ok);
    if (!ok || base < 0)
        return 0;

    double factor = 1.0;
    switch (suffix.unicode()) {
    case 'K': factor = 1024.0; break;
    case 'M': factor = 1024.0 * 1024.0; break;
    case 'G': factor = 1024.0 * 1024.0 * 1024.0; break;
    case 'T': factor = 1024.0 * 1024.0 * 1024.0 * 1024.0; break;
    default:  return 0;
    }

    double bytes = base * factor;
    if (bytes < 0) bytes = 0;
    return static_cast<qulonglong>(bytes);
}

static double bytesToGB(qulonglong bytes)
{
    return double(bytes) / (1024.0 * 1024.0 * 1024.0);
}

QJsonArray mainwindowsiRec::scanSdDevices() const
{
    QJsonArray result;  // จะคืน devices ที่ match sd[a-z]*

    QProcess p;
    QStringList args;
    args << "-J" << "-b" << "-o"
         << "NAME,TYPE,SIZE,MOUNTPOINT,RM";

    p.start("lsblk", args);
    if (!p.waitForFinished(2000)) {
        qWarning() << "[scanSdDevices] lsblk timeout";
        return result;
    }

    QByteArray out = p.readAllStandardOutput();
    if (out.isEmpty()) {
        qWarning() << "[scanSdDevices] lsblk empty output";
        return result;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(out, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[scanSdDevices] parse error:" << err.errorString();
        return result;
    }

    QJsonObject root = doc.object();
    QJsonArray blockdevices = root.value("blockdevices").toArray();

    std::function<void(const QJsonObject &, const QString &)> processNode;
    processNode = [&](const QJsonObject &obj, const QString &parentName) {
        const QString name = obj.value("name").toString();
        const QString type = obj.value("type").toString();

        // ### สนใจเฉพาะ sd[a-z]* ###
        if (!name.startsWith("sd") || name.size() < 3) {
            // ยังต้องลงไปดู children ต่อ เผื่อมีอะไรแปลก ๆ
        } else {
            QChar c = name.at(2);
            if (c < QChar('a') || c > QChar('z')) {
                // ไม่ใช่ sda..sdz → ไม่เก็บ แต่ยังเดิน children ต่อ
            } else if (type == "disk" || type == "part") {
                qulonglong sizeBytes = parseLsblkSize(obj.value("size"));
                QString mountPoint = obj.value("mountpoint").toString();
                bool removable = (obj.value("rm").toInt() == 1);

                QJsonObject d;
                d["name"]       = name;                 // "sda", "sda1"
                d["devPath"]    = "/dev/" + name;       // "/dev/sda1"
                d["type"]       = type;                 // "disk" หรือ "part"
                d["parent"]     = parentName;           // ถ้าเป็น part -> "sda"
                d["sizeBytes"]  = QString::number(sizeBytes);
                d["sizeGB"]     = bytesToGB(sizeBytes);
                d["mountPoint"] = mountPoint;
                d["mounted"]    = !mountPoint.isEmpty();
                d["removable"]  = removable;

                result.append(d);
            }
        }

        // children (partition ของ disk)
        QJsonArray children = obj.value("children").toArray();
        for (const QJsonValue &ch : children) {
            if (!ch.isObject())
                continue;
            processNode(ch.toObject(), name);
        }
    };

    for (const QJsonValue &val : blockdevices) {
        if (!val.isObject())
            continue;
        processNode(val.toObject(), QString());
    }

    return result;
}

bool mainwindowsiRec::mountSdDevice(const QString &devName) const
{
    // devName รับมาเช่น "sda", "sdb", "sda1" อะไรก็ได้
    QString devPath = devName.startsWith("/dev/")
                          ? devName
                          : ("/dev/" + devName);
    QString baseName = devName;
    if (baseName.startsWith("/dev/"))
        baseName = baseName.mid(5);

    // ถ้าชื่อตาม pattern sda1, sdb2, ... → เอาแค่ 3 ตัวแรก
    if (baseName.startsWith("sd") && baseName.size() >= 4)
        baseName = baseName.left(3);  // "sda1" -> "sda"

    // /media/usb_sda , /media/usb_sdb , /media/usb_sda1 , ...
    QString mountPoint = "/media/usb_" + baseName;

    qDebug() << "[mountSdDevice] devPath =" << devPath
             << "mountPoint =" << mountPoint;

    QDir dir;
    if (!dir.mkpath(mountPoint)) {
        qWarning() << "[mountSdDevice] mkpath failed:" << mountPoint;
        return false;
    }

    QProcess p;
    QStringList args;
    args << devPath << mountPoint;

    // NOTE: ต้องรันโปรแกรมด้วย root หรือจัด sudo/pkexec ให้ mount ได้
    p.start("mount", args);
    if (!p.waitForFinished(5000)) {
        qWarning() << "[mountSdDevice] mount timeout for" << devPath;
        return false;
    }

    if (p.exitCode() != 0) {
        qWarning() << "[mountSdDevice] mount error:"
                   << p.readAllStandardError();
        return false;
    }

    qDebug() << "[mountSdDevice] mount success for" << devPath;
    return true;
}

bool mainwindowsiRec::unmountSdDevice(const QString &devName) const
{
    qDebug() << "unmountSdDevice:" << devName;
    QString baseName = devName;
    if (baseName.startsWith("/dev/"))
        baseName = baseName.mid(5);   // "/dev/sda1" -> "sda1"

    // ถ้าเป็น pattern sda1, sdb1, ... จะเอาแค่ 3 ตัวแรกให้ได้ root disk
    // เพื่อให้ path เป็น /media/usb_sda, /media/usb_sdb
    if (baseName.startsWith("sd") && baseName.size() >= 4)
        baseName = baseName.left(3);  // "sda1" -> "sda"

    QString mountPoint = "/media/usb_" + baseName;  // /media/usb_sda, /media/usb_sdb, ...

    qDebug() << "[unmountSdDevice] trying umount" << mountPoint;

    QProcess p;
    QStringList args;
    args << mountPoint;

    // ใช้ umount ตามที่คุณบอก (ไม่ใช่ rm)
    p.start("umount", args);
    if (!p.waitForFinished(5000)) {
        qWarning() << "[unmountSdDevice] umount timeout for" << mountPoint;
        return false;
    }

    if (p.exitCode() != 0) {
        qWarning() << "[unmountSdDevice] umount error:"
                   << p.readAllStandardError();
        return false;
    }

    qDebug() << "[unmountSdDevice] umount success for" << mountPoint;

    // ถ้าไม่อยากให้โฟลเดอร์ค้างอยู่ใน /media และมันว่างเปล่าแล้ว
    // สามารถลบโฟลเดอร์แบบปลอดภัยด้วย rmdir (ไม่ใช่ rm -rf)
    // QDir dir;
    // dir.rmdir(mountPoint); // จะลบได้เฉพาะตอนโฟลเดอร์ว่าง

    return true;
}

bool mainwindowsiRec::unmountAllUsbSd() const
{
    // 0) DEBUG: current working directory
    QString cwd = QDir::currentPath();
    qDebug() << "[unmountAllUsbSd] currentPath =" << cwd;

    // ถ้าอยู่ใน USB → ต้องออกมาก่อน
    if (cwd.startsWith("/media/usb_sd")) {
        QDir::setCurrent("/");
        qDebug() << "[unmountAllUsbSd] changed CWD to /";
    }

    // 1) SYNC ก่อน unmount
    ::sync();
    qDebug() << "[unmountAllUsbSd] sync() done";

    // 2) หา directory usb_sd*
    QDir mediaDir("/media");
    if (!mediaDir.exists()) {
        qWarning() << "[unmountAllUsbSd] /media not exists";
        return false;
    }

    QStringList dirs = mediaDir.entryList(
        QStringList() << "usb_sd*",
        QDir::Dirs | QDir::NoDotAndDotDot);

    if (dirs.isEmpty()) {
        qWarning() << "[unmountAllUsbSd] no usb_sd* dirs found";
        return false;
    }

    bool anyOk = false;

    // 3) Loop unmount ทุกอัน
    for (const QString &dName : dirs) {

        QString mountPoint = mediaDir.absoluteFilePath(dName);
        qDebug() << "[unmountAllUsbSd] try umount" << mountPoint;

        // 3.1) SYNC ต่ออีกรอบ เผื่อ background flush
        ::sync();

        // 3.2) ใช้ fuser เช็กว่า process ไหนจับอยู่ (แบบ verbose)
        QProcess fuser;
        fuser.start("fuser", QStringList() << "-vm" << mountPoint);
        fuser.waitForFinished(1500);

        QString busyOut = QString::fromLocal8Bit(fuser.readAllStandardOutput());
        QString busyErr = QString::fromLocal8Bit(fuser.readAllStandardError());

        if (!busyOut.trimmed().isEmpty() || !busyErr.trimmed().isEmpty()) {
            qWarning() << "[unmountAllUsbSd] fuser output:\n" << busyOut << busyErr;
        }
        // แปลง list ของ PID จาก fuser
        QStringList pids;
        for (const QString &tok : busyOut.split(QRegExp("\\s+"), QString::SkipEmptyParts)) {            bool ok = false;
            int pid = tok.toInt(&ok);
            if (ok) {
                pids << tok;
            }
        }

        if (!pids.isEmpty()) {
            qWarning() << "[unmountAllUsbSd] try kill PIDs:" << pids;
            for (const QString &pid : pids) {
                // ส่ง SIGTERM ก่อน
                QProcess::execute("kill", QStringList() << "-TERM" << pid);
            }
            ::sync();
        }

        // 3.3) ทำการ unmount
        QProcess p;
        p.start("umount", QStringList() << mountPoint);

        if (!p.waitForFinished(5000)) {
            qWarning() << "[unmountAllUsbSd] umount timeout for" << mountPoint;
            continue;
        }

        if (p.exitCode() != 0) {
            qWarning() << "[unmountAllUsbSd] umount error for" << mountPoint
                       << ":" << p.readAllStandardError();
            continue;
        }

        qDebug() << "[unmountAllUsbSd] umount success for" << mountPoint;

        // 3.4) ลบ directory ถ้าว่าง
        mediaDir.rmdir(dName);

        anyOk = true;
    }

    return anyOk;
}

namespace {

struct WavFormat {
    quint16 numChannels = 0;
    quint32 sampleRate = 0;
    quint16 bitsPerSample = 0;
};

// อ่าน .wav (PCM) แล้วดึง format + PCM data ออกมา
static bool readPcmFromWav(const QString &path,
                           WavFormat &fmt,
                           QByteArray &pcmData,
                           QString &errorReason)
{
    errorReason.clear();
    pcmData.clear();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        errorReason = "open_fail: " + path;
        return false;
    }

    QByteArray all = f.readAll();
    int fileSize = all.size();
    if (fileSize < 44) {
        errorReason = "too_small: " + path;
        return false;
    }

    const uchar *p = reinterpret_cast<const uchar*>(all.constData());

    // ---- ตรวจ RIFF + WAVE ----
    if (memcmp(p + 0,  "RIFF", 4) != 0 ||
        memcmp(p + 8,  "WAVE", 4) != 0)
    {
        errorReason = "not_riff_wave: " + path;
        return false;
    }

    int pos = 12;  // เริ่มหลัง "RIFFxxxxWAVE"
    bool gotFmt  = false;
    bool gotData = false;

    quint16 numChannels = 0;
    quint32 sampleRate = 0;
    quint16 bitsPerSample = 0;

    int dataOffset = 0;
    int dataSize   = 0;

    // ---- เดินหา chunk ทั้งหมดแบบ tolerant ----
    while (pos + 8 <= fileSize)
    {
        // chunk id
        char id[5];
        memcpy(id, p + pos, 4);
        id[4] = '\0';
        QString cid = QString::fromLatin1(id);

        // chunk size (little endian)
        quint32 chunkSize = qFromLittleEndian<quint32>(p + pos + 4);

        int chunkDataStart = pos + 8;
        int chunkDataEnd   = chunkDataStart + chunkSize;

        // ป้องกัน overshoot
        if (chunkDataEnd > fileSize) {
            chunkDataEnd = fileSize;     // truncate
            chunkSize = chunkDataEnd - chunkDataStart;
        }

        // ---- fmt ----
        if (cid == "fmt ")
        {
            if (chunkSize < 16) {
                errorReason = "fmt_chunk_too_small: " + path;
                return false;
            }

            const uchar *fp = p + chunkDataStart;
            quint16 audioFormat = qFromLittleEndian<quint16>(fp + 0);

            numChannels    = qFromLittleEndian<quint16>(fp + 2);
            sampleRate     = qFromLittleEndian<quint32>(fp + 4);
            bitsPerSample  = qFromLittleEndian<quint16>(fp + 14);

            if (audioFormat != 1) {
                errorReason = "not_pcm: " + path;
                return false;
            }

            gotFmt = true;
        }

        // ---- data ----
        else if (cid == "data")
        {
            dataOffset = chunkDataStart;
            dataSize   = chunkSize;
            gotData    = true;
            break;  // พอแล้ว
        }

        // ---- chunk อื่น ๆ ที่ไม่สนใจ ----
        // เช่น JUNK, bext, LIST, fact ฯลฯ
        pos = chunkDataEnd;
    }

    if (!gotFmt) {
        errorReason = "no_fmt_chunk: " + path;
        return false;
    }
    if (!gotData) {
        errorReason = "no_data_chunk: " + path;
        return false;
    }

    // ---- ดึง PCM ----
    pcmData = QByteArray(reinterpret_cast<const char*>(p + dataOffset), dataSize);

    // ---- ส่งออก format ----
    fmt.numChannels   = numChannels;
    fmt.sampleRate    = sampleRate;
    fmt.bitsPerSample = bitsPerSample;

    return true;
}


// สร้าง header WAV (PCM) จาก format + dataSize
static QByteArray makeWavHeader(const WavFormat &fmt, quint32 dataSize)
{
    QByteArray hdr;
    hdr.resize(44);
    uchar *h = reinterpret_cast<uchar*>(hdr.data());

    quint32 byteRate   = fmt.sampleRate * fmt.numChannels * (fmt.bitsPerSample / 8);
    quint16 blockAlign = fmt.numChannels * (fmt.bitsPerSample / 8);
    quint32 chunkSize  = 36 + dataSize; // 4 + (8+Subchunk1Size) + (8+Subchunk2Size)
    quint32 subchunk1Size = 16;         // PCM
    quint16 audioFormat   = 1;          // PCM

    // "RIFF"
    memcpy(h + 0, "RIFF", 4);
    qToLittleEndian(chunkSize,   h + 4);
    memcpy(h + 8, "WAVE", 4);

    // "fmt "
    memcpy(h + 12, "fmt ", 4);
    qToLittleEndian(subchunk1Size, h + 16);
    qToLittleEndian(audioFormat,   h + 20);
    qToLittleEndian(fmt.numChannels, h + 22);
    qToLittleEndian(fmt.sampleRate,  h + 24);
    qToLittleEndian(byteRate,       h + 28);
    qToLittleEndian(blockAlign,     h + 32);
    qToLittleEndian(fmt.bitsPerSample, h + 34);

    // "data"
    memcpy(h + 36, "data", 4);
    qToLittleEndian(dataSize, h + 40);

    return hdr;
}

}

// namespace
bool mainwindowsiRec::mergeAndConvertToMp3(const QStringList &inputs,
                                           const QString     &outDir,
                                           const QString     &baseName,
                                           QString           &outFinalPath,
                                           QString           &errorReason)
{
    outFinalPath.clear();
    errorReason.clear();

    if (inputs.isEmpty()) {
        errorReason = "no_input_files";
        return false;
    }

    // 1) ตรวจไฟล์ และอ่าน WAV + PCM
    emit exportProgress(5, QStringLiteral("Validating input files..."));

    QVector<QString> existing;
    existing.reserve(inputs.size());

    for (const QString &p : inputs) {
        QFileInfo fi(p);
        if (!fi.exists()) {
            qWarning() << "[mergeAndConvertToMp3] input not exists:" << p;
            continue;
        }
        existing.push_back(fi.absoluteFilePath());
    }

    if (existing.isEmpty()) {
        errorReason = "no_existing_files";
        return false;
    }

    // 2) เรียงตามเวลา (mtime) = เก่าสุด → ใหม่สุด
    struct FileEntry {
        QString   path;
        QDateTime mtime;
    };
    QVector<FileEntry> vec;
    vec.reserve(existing.size());
    for (const QString &p : existing) {
        QFileInfo fi(p);
        FileEntry e;
        e.path  = fi.absoluteFilePath();
        e.mtime = fi.lastModified();
        vec.push_back(e);
    }

    std::sort(vec.begin(), vec.end(), [](const FileEntry &a, const FileEntry &b){
        return a.mtime < b.mtime;
    });

    QStringList sortedInputs;
    for (const FileEntry &e : vec)
        sortedInputs << e.path;

    qDebug() << "[mergeAndConvertToMp3] sorted inputs =" << sortedInputs;

    // 3) อ่าน WAV แต่ละไฟล์ → เช็ค format + ดึง PCM data มาต่อกัน
    emit exportProgress(15, QStringLiteral("Reading and concatenating WAV data..."));

    WavFormat baseFmt;
    bool baseFmtSet = false;
    QByteArray mergedPcm;

    for (const QString &p : sortedInputs) {
        WavFormat fmt;
        QByteArray pcm;
        QString err;

        if (!readPcmFromWav(p, fmt, pcm, err)) {
            qWarning() << "[mergeAndConvertToMp3] readPcmFromWav failed:" << err;
            errorReason = err;
            return false;
        }

        if (!baseFmtSet) {
            baseFmt = fmt;
            baseFmtSet = true;
        } else {
            // ต้องมี format เดียวกันหมด
            if (fmt.numChannels   != baseFmt.numChannels ||
                fmt.sampleRate    != baseFmt.sampleRate ||
                fmt.bitsPerSample != baseFmt.bitsPerSample)
            {
                qWarning() << "[mergeAndConvertToMp3] format mismatch:" << p;
                errorReason = QStringLiteral("format_mismatch: %1").arg(p);
                return false;
            }
        }

        mergedPcm.append(pcm);
    }

    if (!baseFmtSet || mergedPcm.isEmpty()) {
        errorReason = "no_pcm_data";
        return false;
    }

    // 4) เตรียมโฟลเดอร์ outDir + โฟลเดอร์ย่อยตามวันที่วันนี้ (yyyyMMdd)
    QDir baseDir(outDir);
    if (!baseDir.exists()) {
        if (!baseDir.mkpath(".")) {
            errorReason = QStringLiteral("cannot_mkpath_%1").arg(outDir);
            return false;
        }
    }

    // โฟลเดอร์วันที่วันนี้ เช่น "20251124"
    const QString dateFolderName =
        QDateTime::currentDateTime().date().toString("yyyyMMdd");

    // path ของโฟลเดอร์วันที่
    const QString dateFolderPath = baseDir.filePath(dateFolderName);

    QDir dateDir(dateFolderPath);
    if (!dateDir.exists()) {
        if (!baseDir.mkpath(dateFolderName)) {
            errorReason = QStringLiteral("cannot_mkpath_date_folder_%1")
            .arg(dateFolderPath);
            return false;
        }
    }

    // ใช้ baseName เดิม
    QString base = baseName.isEmpty()
                       ? QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")
                       : baseName;

    // ❗ ตอนนี้ให้ tmp wav และ mp3 ไปอยู่ในโฟลเดอร์วันที่
    QString tmpWavPath = dateDir.absoluteFilePath(base + "_tmp_merge.wav");
    QString outMp3Path = dateDir.absoluteFilePath(base + ".mp3");

    qDebug() << "[mergeAndConvertToMp3] tmpWavPath =" << tmpWavPath;
    qDebug() << "[mergeAndConvertToMp3] outMp3Path =" << outMp3Path;

    // 5) เขียน header + merged PCM เป็น .wav
    emit exportProgress(40, QStringLiteral("Writing merged WAV..."));

    QByteArray hdr = makeWavHeader(baseFmt, static_cast<quint32>(mergedPcm.size()));
    QFile outWav(tmpWavPath);
    if (!outWav.open(QIODevice::WriteOnly)) {
        errorReason = QStringLiteral("cannot_open_tmp_wav: %1").arg(tmpWavPath);
        return false;
    }
    outWav.write(hdr);
    outWav.write(mergedPcm);
    outWav.close();

    qDebug() << "[mergeAndConvertToMp3] merged wav written:" << tmpWavPath;

    // 6) เรียก ffmpeg แปลง wav → mp3
    emit exportProgress(60, QStringLiteral("Encoding MP3 with ffmpeg..."));

    QStringList args;
    args << "-y";
    args << "-i" << tmpWavPath;
    args << "-codec:a" << "libmp3lame";
    args << "-b:a"    << "192k";
    args << outMp3Path;

    qDebug() << "[mergeAndConvertToMp3] ffmpeg args =" << args;
    int ret = QProcess::execute("ffmpeg", args);
    if (ret != 0) {
        errorReason = QString("ffmpeg_failed_exit_%1").arg(ret);
        emit exportProgress(100, "Export failed (ffmpeg).");
        emit exportFinished(false, QString(), errorReason);
        return false;
    }

    qDebug() << "[mergeAndConvertToMp3] encoded mp3:" << outMp3Path;

    // 🔹 flush ให้เคอร์เนลเขียนข้อมูลลง disk
    ::sync();
    qDebug() << "[mergeAndConvertToMp3] sync() done";

    outFinalPath = outMp3Path;
    emit exportProgress(100, "Export completed");
    emit exportFinished(true, outFinalPath, QString());
    return true;
}

void mainwindowsiRec::recordDeviceLiveStream(QString megs, QWebSocket* wClient) {
    qDebug() << "recordDeviceLiveStream:" << megs;

}

void mainwindowsiRec::deviceStatus(QString megs)
{
    qDebug() << "deviceStatus:" << megs;

    QStringList parts = megs.split(",");
    if (parts.size() < 4) {
        qWarning() << "Invalid message format:" << megs;
        return;
    }

    QString ip     = parts[0].trimmed();
    QString freq   = parts[1].trimmed();
    QString uri    = parts[2].trimmed();
    QString action = parts[3].trimmed();

    QString validAction;
    if (action == "PAUSE" ||
        action == "ANNOUNCE" ||
        action == "RECORD" ||
        action == "GET_PARAMETER") {
        validAction = action;
    } else {
        validAction = "UNKNOWN";
    }

    // ---------- ตรงนี้: handle logic RECORD / PAUSE เพิ่ม ----------
    if (validAction == "RECORD") {
        handleRecordAction(ip, freq, uri);
    } else if (validAction == "PAUSE") {
        handlePauseAction(ip, freq, uri);
    }
}

void mainwindowsiRec::onUnixSocketMessage(const QString &msg)
{
    qDebug() << "mainwindows: Received message from socket:" << msg;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "[UnixSocket] JSON parse error:" << err.errorString()
        << "msg =" << msg;
    }

    //    recordDeviceLiveStream(msg, wClient);
    qDebug() << "onUnixSocketMessage_m_currentWClient:" << m_currentWClient << msg << wClient;
    deviceStatus(msg);

}

void mainwindowsiRec::handleRecordAction(const QString &ip,
                                         const QString &freq,
                                         const QString &uri)
{

    qDebug() << "[handleRecordAction] RECORD from"
             << "ip =" << ip
             << "freq =" << freq
             << "uri =" << uri;
}

void mainwindowsiRec::handlePauseAction(const QString &ip,
                                        const QString &freq,
                                        const QString &uri)
{
    if (!m_recordFilesPageActive) {
        qDebug() << "[handlePauseAction] PAUSE ignored because RecordFiles page not active";
        return;
    }

    qDebug() << "[handlePauseAction] PAUSE from"
             << "ip =" << ip
             << "freq =" << freq
             << "uri =" << uri
             << "-> will emit recordFileMayBeReady in 5s";

    QTimer::singleShot(5000, this, [this]() {
        qDebug() << "[handlePauseAction] 5s passed, emitting recordFileMayBeReady";
        mysql->upDateTableFileRecord();
    });
}


void mainwindowsiRec::getSystemPage(QWebSocket *webSender)
{
    qDebug() << "getSystemPage:" << webSender;
    int dateTimeMethod;
    if (ntp){
        dateTimeMethod = 1;
    }else{
        dateTimeMethod = 2;
    }

    QString message = QString("{\"menuID\":\"system\", \"SwVersion\":\"%1\", \"HwVersion\":\"%2\", \"dateTimeMethod\":\"%3\", \"ntpServer\":\"%4\", \"location\":\"%5\", \"inviteMode\":%6}")
                          .arg(SwVersion).arg(HwVersion).arg(dateTimeMethod).arg(ntpServer).arg(timeLocation).arg(inviteMode);
    SocketServeriGate->sendToWebMessageClientWebSender(message,webSender);
}


void mainwindowsiRec::recLogging(int softPhoneID, int recorderID,QString recState, QString message)
{
    qDebug() << "SocketServeriGate_recLogging" << softPhoneID << recorderID << recState << message;
    SocketServeriGate->sendToWebMessageClient(message);
}
void* mainwindowsiRec::ThreadFunc(void* pTr)
{
    mainwindowsiRec* pThis = static_cast<mainwindowsiRec*>(pTr);
    while (true) {
        emit pThis->getDateTime();
        QThread::msleep(1000);
    }
    return nullptr;
}
void* mainwindowsiRec::ThreadFuncDateTime(void* pTr)
{
    mainwindowsiRec* pThis = static_cast<mainwindowsiRec*>(pTr);
    qDebug() << "ThreadFuncDateTime start";
    while (pThis->m_threadRunning) {
        if (pThis->m_qmlConnected.load(std::memory_order_relaxed)) {
            //            pThis->calendar();
        }
        QThread::msleep(1000);
    }
    qDebug() << "ThreadFuncDateTime exit";
    return nullptr;
}
void* mainwindowsiRec::ThreadFuncFan(void* pTr)
{
    mainwindowsiRec* pThis = static_cast<mainwindowsiRec*>(pTr);
    while (true) {
        pThis->max31760->tempDetect();
        QThread::msleep(1000);
    }
    return NULL;
}
void* mainwindowsiRec::ThreadFunc4(void* pTr)
{
    mainwindowsiRec* pThis = static_cast<mainwindowsiRec*>(pTr);
    pThis->mysql->checkFlieAndRemoveDB();
}
