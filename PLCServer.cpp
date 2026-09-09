#include "PLCServer.h"
#if defined(__GLIBC__)
#include <malloc.h>
#endif

#include <unistd.h>
#include <QAbstractSocket>
#include <QWebSocket>

namespace {

qint64 readProcStatusKb(const QByteArray &key)
{
    QFile file("/proc/self/status");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return -1;

    while (!file.atEnd()) {
        const QByteArray line = file.readLine();

        if (line.startsWith(key)) {
            const QList<QByteArray> parts = line.simplified().split(' ');
            if (parts.size() >= 2)
                return parts.at(1).toLongLong();
        }
    }

    return -1;
}

qint64 readMemInfoKb(const QByteArray &key)
{
    QFile file("/proc/meminfo");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return -1;

    while (!file.atEnd()) {
        const QByteArray line = file.readLine();

        if (line.startsWith(key)) {
            const QList<QByteArray> parts = line.simplified().split(' ');
            if (parts.size() >= 2)
                return parts.at(1).toLongLong();
        }
    }

    return -1;
}

template <typename T>
void releaseQtContainer(T &v)
{
    T empty;
    v.swap(empty);
}

template <typename T>
void releaseStdVector(std::vector<T> &v)
{
    std::vector<T> empty;
    v.swap(empty);
}

template <typename T>
void releaseNestedQList(QList<QList<T>> &v)
{
    for (int i = 0; i < v.size(); ++i) {
        QList<T> empty;
        v[i].swap(empty);
    }

    QList<QList<T>> empty;
    v.swap(empty);
}

void releaseString(QString &s)
{
    QString empty;
    s.swap(empty);
}

void cleanupSocketPointerList(QList<QWebSocket *> &list)
{
    QList<QWebSocket *> activeList;

    for (int i = 0; i < list.size(); ++i) {
        QWebSocket *socket = list.at(i);

        if (socket && socket->state() != QAbstractSocket::UnconnectedState) {
            activeList.append(socket);
        }
    }

    list.swap(activeList);
}

bool dropLinuxCache()
{
    if (::geteuid() != 0) {
        qWarning() << "[clearAppCacheAndSwap] skip drop_caches: process is not root";
        return false;
    }

    ::sync();

    QFile dropFile("/proc/sys/vm/drop_caches");
    if (dropFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (dropFile.write("3\n") > 0) {
            dropFile.flush();
            dropFile.close();
            qDebug() << "[clearAppCacheAndSwap] drop_caches done by direct write";
            return true;
        }

        dropFile.close();
    }

    const int ret = QProcess::execute(
        "sh",
        QStringList() << "-c" << "sync; echo 3 > /proc/sys/vm/drop_caches"
        );

    if (ret == 0) {
        qDebug() << "[clearAppCacheAndSwap] drop_caches done by shell";
        return true;
    }

    qWarning() << "[clearAppCacheAndSwap] drop_caches failed ret =" << ret;
    return false;
}

bool clearLinuxSwapSafely()
{
    if (::geteuid() != 0) {
        qWarning() << "[clearAppCacheAndSwap] skip clear swap: process is not root";
        return false;
    }

    const qint64 memAvailableKb = readMemInfoKb("MemAvailable:");
    const qint64 swapTotalKb    = readMemInfoKb("SwapTotal:");
    const qint64 swapFreeKb     = readMemInfoKb("SwapFree:");

    if (swapTotalKb <= 0) {
        qDebug() << "[clearAppCacheAndSwap] no swap configured";
        return false;
    }

    const qint64 swapUsedKb = swapTotalKb - swapFreeKb;

    if (swapUsedKb <= 0) {
        qDebug() << "[clearAppCacheAndSwap] swap already empty";
        return false;
    }

    // กันเครื่องค้าง: ต้องมี RAM ว่างมากกว่า swap ที่ใช้ + margin 256MB
    const qint64 safetyMarginKb = 256LL * 1024LL;

    if (memAvailableKb < (swapUsedKb + safetyMarginKb)) {
        qWarning() << "[clearAppCacheAndSwap] skip clear swap: not enough MemAvailable"
                   << "MemAvailable(KB)=" << memAvailableKb
                   << "SwapUsed(KB)=" << swapUsedKb;
        return false;
    }

    qDebug() << "[clearAppCacheAndSwap] clear swap start"
             << "MemAvailable(KB)=" << memAvailableKb
             << "SwapUsed(KB)=" << swapUsedKb;

    int ret = QProcess::execute("swapoff", QStringList() << "-a");
    if (ret != 0) {
        qWarning() << "[clearAppCacheAndSwap] swapoff failed ret =" << ret;
        return false;
    }

    ret = QProcess::execute("swapon", QStringList() << "-a");
    if (ret != 0) {
        qWarning() << "[clearAppCacheAndSwap] swapon failed ret =" << ret;
        return false;
    }

    qDebug() << "[clearAppCacheAndSwap] clear swap done";
    return true;
}

bool isSwapUsageHigh(qint64 minSwapUsedKb = 256LL * 1024LL,
                     int maxSwapPercent = 30)
{
    const qint64 memAvailableKb = readMemInfoKb("MemAvailable:");
    const qint64 swapTotalKb    = readMemInfoKb("SwapTotal:");
    const qint64 swapFreeKb     = readMemInfoKb("SwapFree:");

    if (swapTotalKb <= 0)
        return false;

    const qint64 swapUsedKb = swapTotalKb - swapFreeKb;

    if (swapUsedKb <= 0)
        return false;

    const int swapPercent = static_cast<int>((swapUsedKb * 100) / swapTotalKb);

    qDebug() << "[isSwapUsageHigh]"
             << "MemAvailable(KB)=" << memAvailableKb
             << "SwapTotal(KB)=" << swapTotalKb
             << "SwapFree(KB)=" << swapFreeKb
             << "SwapUsed(KB)=" << swapUsedKb
             << "SwapUsed(%)=" << swapPercent;

    if (swapUsedKb >= minSwapUsedKb)
        return true;

    if (swapPercent >= maxSwapPercent)
        return true;

    return false;
}

void removeRawDataFiles()
{
    QDir dir(QStringLiteral("/home/pi/Rawdata"));
    if (!dir.exists()) {
        return;
    }

    const QFileInfoList files = dir.entryInfoList(QStringList{QStringLiteral("data*.raw"),
                                                              QStringLiteral("data*.download")},
                                                  QDir::Files | QDir::NoSymLinks,
                                                  QDir::Name);
    for (const QFileInfo &fileInfo : files) {
        QFile::remove(fileInfo.absoluteFilePath());
    }
}

} // namespace


void PLCServer::clearAppCacheAndSwap(bool dropSystemCache, bool clearSwap)
{
    const qint64 vmRssBefore   = readProcStatusKb("VmRSS:");
    const qint64 rssAnonBefore = readProcStatusKb("RssAnon:");

    const qint64 memAvailableBefore = readMemInfoKb("MemAvailable:");
    const qint64 cachedBefore       = readMemInfoKb("Cached:");
    const qint64 swapTotalBefore    = readMemInfoKb("SwapTotal:");
    const qint64 swapFreeBefore     = readMemInfoKb("SwapFree:");

    qDebug() << "[clearAppCacheAndSwap] BEFORE"
             << "VmRSS(KB)=" << vmRssBefore
             << "RssAnon(KB)=" << rssAnonBefore
             << "MemAvailable(KB)=" << memAvailableBefore
             << "Cached(KB)=" << cachedBefore
             << "SwapUsed(KB)=" << (swapTotalBefore - swapFreeBefore);

    // =========================================================
    // 1) Clear temporary runtime buffers only
    //    ห้ามล้าง Pattern working data ที่ผู้ใช้กำลังใช้งานอยู่
    //    Pattern จะล้างได้เฉพาะตอน New Pattern หรือ Open Pattern เท่านั้น
    // =========================================================

    // QJsonArray: temporary raw / graph / backup
    // หมายเหตุ: patternNoneSmoothA/B/C ตอนนี้ไม่เห็นจุดใช้งานอื่นชัดเจน
    // ถ้าในอนาคตนำไปใช้กับ Pattern UI ห้ามล้างตรงนี้เหมือนกัน
    releaseQtContainer(patternNoneSmoothA);
    releaseQtContainer(patternNoneSmoothB);
    releaseQtContainer(patternNoneSmoothC);

    releaseQtContainer(voltBkup);
    releaseQtContainer(distBkup);
    releaseQtContainer(voltBkupRemote);
    releaseQtContainer(distBkupRemote);

    // Graph event ชั่วคราว ล้างได้
    releaseQtContainer(distA);
    releaseQtContainer(voltA);
    releaseQtContainer(distB);
    releaseQtContainer(voltB);
    releaseQtContainer(distC);
    releaseQtContainer(voltC);

    // =========================================================
    // IMPORTANT:
    // ห้ามล้าง Pattern persistent/current data ตรงนี้
    // เพราะ clearAppCacheAndSwap() ถูกเรียกอัตโนมัติทุก 30 นาที
    // ถ้าล้างตรงนี้ ข้อมูล Pattern จะหายโดยผู้ใช้ไม่ได้กด New/Open
    // =========================================================

    // DO NOT CLEAR HERE:
    // distAPat, voltAPat, distBPat, voltBPat, distCPat, voltCPat
    // disAPattern, disBPattern, disCPattern
    // volAPattern, volBPattern, volCPattern
    // distanceArrayAPattern, distanceArrayBPattern, distanceArrayCPattern
    // distanceArrayAPatternbkup, distanceArrayBPatternbkup, distanceArrayCPatternbkup
    // voltageArrayAPattern, voltageArrayBPattern, voltageArrayCPattern
    // voltageArrayAPatternbkup, voltageArrayBPatternbkup, voltageArrayCPatternbkup

    // QList<QList<double>> / QList<QList<float>>
    // พวกนี้เป็น buffer ระหว่างคำนวณ graph/pattern capture ถ้ากำลัง capture อยู่ก็อาจกระทบได้
    // ถ้าต้องการปลอดภัยกว่า ให้ล้างเฉพาะเมื่อไม่ได้อยู่ใน Pattern mode
    if (!patternSelected && !interlockPattern) {
        releaseNestedQList(voltageListA);
        releaseNestedQList(maxVoltageListA);
        releaseNestedQList(voltageListB);
        releaseNestedQList(maxVoltageListB);
        releaseNestedQList(voltageListC);
        releaseNestedQList(maxVoltageListC);

        releaseNestedQList(kmListA);
        releaseNestedQList(maxKmListA);
        releaseNestedQList(kmListB);
        releaseNestedQList(maxKmListB);
        releaseNestedQList(kmListC);
        releaseNestedQList(maxKmListC);

        releaseStdVector(resultMaxListA);
        releaseStdVector(resultMaxListB);
        releaseStdVector(resultMaxListC);
    }

    // Surge/temp buffer ล้างได้
    releaseStdVector(resultMaxListSurge);
    releaseStdVector(resultMaxListSurgeRemote);

    // QString raw buffer ขนาดใหญ่
    // ล้างเฉพาะ non-pattern raw data
    releaseString(rawdataSurge);
    releaseString(rawdataArrayA);
    releaseString(rawdataArrayB);
    releaseString(rawdataArrayC);

    // DO NOT CLEAR HERE:
    // rawdataPatternArrayA
    // rawdataPatternArrayB
    // rawdataPatternArrayC

    releaseString(testNoneSmoothA);
    releaseString(testNoneSmoothB);
    releaseString(testNoneSmoothC);

    // =========================================================
    // 2) Cleanup pointer list เฉพาะ socket ที่หลุดแล้ว
    //    ห้าม delete socket หลักที่ระบบยังใช้งานอยู่
    // =========================================================
    cleanupSocketPointerList(Monitor_address);
    cleanupSocketPointerList(webapp_address);

#if defined(__GLIBC__)
    ::malloc_trim(0);
#endif

    // =========================================================
    // 3) Drop Linux cache ทุก 8 ชั่วโมงเท่านั้น
    // =========================================================
    static QDateTime lastSystemDropCacheAt;
    const QDateTime now = QDateTime::currentDateTime();

    const qint64 dropIntervalMs = 8LL * 60LL * 60LL * 1000LL;

    const bool shouldDropSystemCache =
        dropSystemCache &&
        (!lastSystemDropCacheAt.isValid() ||
         lastSystemDropCacheAt.msecsTo(now) >= dropIntervalMs);

    if (shouldDropSystemCache) {
        if (dropLinuxCache()) {
            lastSystemDropCacheAt = now;
        }
    }

    // =========================================================
    // 4) Clear swap แบบ safe mode
    //    แนะนำเรียกเฉพาะตอนจำเป็น เพราะ swapoff อาจทำให้เครื่องหน่วง
    // =========================================================
    if (clearSwap) {
        clearLinuxSwapSafely();
    }

#if defined(__GLIBC__)
    ::malloc_trim(0);
#endif

    const qint64 vmRssAfter   = readProcStatusKb("VmRSS:");
    const qint64 rssAnonAfter = readProcStatusKb("RssAnon:");

    const qint64 memAvailableAfter = readMemInfoKb("MemAvailable:");
    const qint64 cachedAfter       = readMemInfoKb("Cached:");
    const qint64 swapTotalAfter    = readMemInfoKb("SwapTotal:");
    const qint64 swapFreeAfter     = readMemInfoKb("SwapFree:");

    qDebug() << "[clearAppCacheAndSwap] AFTER"
             << "VmRSS(KB)=" << vmRssAfter
             << "RssAnon(KB)=" << rssAnonAfter
             << "MemAvailable(KB)=" << memAvailableAfter
             << "Cached(KB)=" << cachedAfter
             << "SwapUsed(KB)=" << (swapTotalAfter - swapFreeAfter);
}

PLCServer::PLCServer(QObject * parent): QObject(parent) {
    getReadyFolder();
    checkFolderExist();
    removeRawDataFiles();
    Email_Config = new EmailConfig;
    Email_Param = new EMAIL_Param;
    Email_ParamTemp = new EMAIL_Param;
    SetupEquipment = new SetupParameterEquipment;
    networks = new Network;
    networksTemp = new Network;
    delays = new INPUT_DELAY;
    temp_delays = new INPUT_DELAY;
    FTP_Param_ = new FTP_Param;
    eventHistory = new EventAlarmHistory;
    eventHistory -> setup();
    version = new Version;
    getSetting();
    selectProgram(language);
    networking = new NetworkMng();
    manager = new QNetworkAccessManager(this);
    //    QThread::msleep(1000);
    //    ss = new ChatServerTest(55555);
    server = new ChatServer(5520);
    client = new SocketClient();
    plc_client = new SocketClient();
    myDatabase = new Database("ITouch", "pi", "rpi3!2024", "127.0.0.1", this);
    //    datastorage =section new DataStorage();
    OpenPLC_param = new Param;
    Monitor_param = new Param;
    FPGA_param = new Param;
    snmp_param = new Param;
    PLCserver_param = new Param;

    Aux = new AUX;
    tempAux = new AUX;

    OpenPLC_IO = new IO;
    Monitor_IO = new IO;
    FPGA_IO = new IO;
    snmp_IO = new IO;
    gps = new GPS;

    loopWaitPic = new QTimer();
    loopNetworkTimer = new QTimer();
    recheckParam = new QTimer();
    loopTimer = new QTimer();
    patternTimer = new QTimer();
    loopLogout = new QTimer();
    loopLogoutCounter = new QTimer();
    loopLogoutVNC = new QTimer();
    loopLogoutCounterVNC = new QTimer();
    snmp_address = new QWebSocket();
    OpenPLC_address = new QWebSocket();
    INPUT_PLC_address = new QWebSocket();
    FPGA_address = new QWebSocket();
    PLCserver_address = new QWebSocket();

    day = new Days;
    lastStateday = new Days;
    snmp_address -> ignoreSslErrors();
    OpenPLC_address -> ignoreSslErrors();
    //    Monitor_address->ignoreSslErrors();
    FPGA_address -> ignoreSslErrors();
    PLCserver_address -> ignoreSslErrors();
    OpenPLC_count = 0;
    snmp_count = 0;
    FPGA_count = 0;
    input_count = 0;
    plcServer_count = 0;

    operateTime = new QTimer();
    screenPictureDelayTimer = new QTimer(this);

    //    OpenPLC_param->PLC_DI_ERROR = true;
    //    qDebug() << "fail:" << OpenPLC_param->fn_fail();
    //    qDebug() << "operate:" << OpenPLC_param->fn_operate();
    //    OpenPLC_param->MANUAL_TEST_EVENT = true;
    //    qDebug() << "operate:" << OpenPLC_param->fn_operate();
    //    //server
    //    connect(ss, SIGNAL(newCommandProcess(QString, QWebSocket*)), this, SLOT(manageData(QString, QWebSocket*)));
    //    connect(ss, SIGNAL(disconnected(QWebSocket *)), this, SLOT(disconnected(QWebSocket *)));
    //server
    // connect(server, SIGNAL(newCommandProcess(QString, QWebSocket *)),
    //         this, SLOT(manageData(QString, QWebSocket *)), Qt::QueuedConnection);

    // connect(server, SIGNAL(disconnected(QWebSocket *)),
    //         this, SLOT(disconnected(QWebSocket *)), Qt::QueuedConnection);
    connect(server, SIGNAL(newCommandProcess(QString, QWebSocket * )), this, SLOT(manageData(QString, QWebSocket * )));
    connect(server, SIGNAL(disconnected(QWebSocket * )), this, SLOT(disconnected(QWebSocket * )));

    //client
    connect(client, SIGNAL(newCommandProcess(QString)), this, SLOT(manageDataClient(QString)));
    connect(client, SIGNAL(SocketClientError()), this, SLOT(SocketClientError()));

   connect(plc_client,SIGNAL(newCommandProcess(QString)),this,SLOT(manageDataClientPLC(QString)));
   connect(plc_client,SIGNAL(SocketClientError()),this,SLOT(SocketClientError()));

    //this
    connect(this, SIGNAL(sendMessage(QString, QWebSocket * )), server, SLOT(sendMessage(QString, QWebSocket * )));
    connect(this, SIGNAL(broadcastMessage(QString)), server, SLOT(broadcastMessage(QString)));
    connect(this, SIGNAL(updateUser(QString)), myDatabase, SLOT(userMode(QString)));
    connect(this, SIGNAL(setNTPServer(QString)), myDatabase, SLOT(setNTPServer(QString)));
    connect(this, SIGNAL(updateRelay(QString)), myDatabase, SLOT(storeStatusAux(QString)));
    connect(this, SIGNAL(getDistanceandDetailA(QString)), myDatabase, SLOT(DistanceandDetailPhaseA(QString)));
    connect(this, SIGNAL(getDistanceandDetailB(QString)), myDatabase, SLOT(DistanceandDetailPhaseB(QString)));
    connect(this, SIGNAL(getDistanceandDetailC(QString)), myDatabase, SLOT(DistanceandDetailPhaseC(QString)));
    connect(this, SIGNAL(getTablePhaseA(QString)), myDatabase, SLOT(getMySqlPhaseA(QString)));
    connect(this, SIGNAL(getTablePhaseB(QString)), myDatabase, SLOT(getMySqlPhaseB(QString)));
    connect(this, SIGNAL(getTablePhaseC(QString)), myDatabase, SLOT(getMySqlPhaseC(QString)));
    connect(this, SIGNAL(deletedMySQLA(QString)), myDatabase, SLOT(deletedDataMySQLPhaseA(QString)));
    connect(this, SIGNAL(deletedMySQLB(QString)), myDatabase, SLOT(deletedDataMySQLPhaseB(QString)));
    connect(this, SIGNAL(deletedMySQLC(QString)), myDatabase, SLOT(deletedDataMySQLPhaseC(QString)));
    connect(this, SIGNAL(updateTablePhaseA(QString)), myDatabase, SLOT(updateTablePhaseA(QString)));
    connect(this, SIGNAL(updateTablePhaseB(QString)), myDatabase, SLOT(updateTablePhaseB(QString)));
    connect(this, SIGNAL(updateTablePhaseC(QString)), myDatabase, SLOT(updateTablePhaseC(QString)));

    connect(this, SIGNAL(parameterThreshold(QString)), myDatabase, SLOT(configParemeterThreshold(QString)));
    connect(this, SIGNAL(getDataThreshold()), myDatabase, SLOT(getThreshold()));
    connect(this, SIGNAL(settingGeneral()), myDatabase, SLOT(getSettingInfo()));
    connect(this, SIGNAL(preiodicSetting()), myDatabase, SLOT(getpreiodicInfo()));
    connect(this, SIGNAL(updateTimer(QString)), myDatabase, SLOT(getUpdatePeriodic(QString)));
    connect(this, SIGNAL(updateWeekly(QString)), myDatabase, SLOT(getUpdateWeekly(QString)));
    //    connect(this, SIGNAL(rawdataPlot(QString)), myDatabase, SLOT(getRawData(QString)));
    connect(this, SIGNAL(cursorDistance(QString)), myDatabase, SLOT(getPositionDistance(QString)));
    connect(this, SIGNAL(moveCursor(QString)), myDatabase, SLOT(controlCursor(QString)));
    connect(this, SIGNAL(changeDistanceRange(QString)), myDatabase, SLOT(getChangeDistance(QString)));
    connect(this, SIGNAL(taggingpoint(QString)), myDatabase, SLOT(taggingpoint(QString)));
    connect(this, SIGNAL(settingdisplay(QString)), myDatabase, SLOT(SettingDisplay(QString)));
    connect(this, SIGNAL(sendToMonitor(QString)), server, SLOT(sendMessageToMonitor(QString)));
    // connect(this, SIGNAL(sendToMonitor(QString)), server, SLOT(sendMessageToVNC(QString)));
    connect(this, SIGNAL(sendToVNC(QString)), server, SLOT(sendMessageToVNC(QString)));
    connect(this, SIGNAL(sendToSocket(QString)), client, SLOT(sendMessage(QString)));
    connect(this, SIGNAL(sendToSocketPLC(QString)), plc_client, SLOT(sendMessage(QString)));
    connect(this, SIGNAL(clearDisplay(QString)), myDatabase, SLOT(cleanDataInGraph(QString)));

    connect(this, SIGNAL(UpdateMarginSettingParameter(QString)), myDatabase, SLOT(UpdateMarginSettingParameter(QString)));
    connect(this, SIGNAL(updataListOfMarginA(QString)), myDatabase, SLOT(updataListOfMarginA(QString)));
    connect(this, SIGNAL(updataListOfMarginB(QString)), myDatabase, SLOT(updataListOfMarginB(QString)));
    connect(this, SIGNAL(updataListOfMarginC(QString)), myDatabase, SLOT(updataListOfMarginC(QString)));
    connect(this, SIGNAL(parameterMarginA(QString)), myDatabase, SLOT(configParemeterMarginA(QString)));
    connect(this, SIGNAL(parameterMarginB(QString)), myDatabase, SLOT(configParemeterMarginB(QString)));
    connect(this, SIGNAL(parameterMarginC(QString)), myDatabase, SLOT(configParemeterMarginC(QString)));
    connect(this, SIGNAL(getEditDatafromMySQLA(QString)), myDatabase, SLOT(edittingMysqlA(QString)));
    connect(this, SIGNAL(uploadCSVTower()), myDatabase, SLOT(uploadCSVTower()));
    connect(this, SIGNAL(updatePathNotMount(QString, QString)), myDatabase, SLOT(updatePathNotMount(QString, QString)));
    connect(this, SIGNAL(calldeleteOldFilesAndRecords()), myDatabase, SLOT(deleteOldFilesAndRecords()));
    connect(this, SIGNAL(updateSetupEquipment(SetupParameterEquipment * )), myDatabase, SLOT(updateSetupEquipment(SetupParameterEquipment * )));

    //    connect(this, SIGNAL(parameterMarginA(QString)), myDatabase, SLOT(UpdateMarginSettingParameter(QString)));
    //    connect(this, SIGNAL(parameterMarginB(QString)), myDatabase, SLOT(UpdateMarginSettingParameter(QString)));
    //    connect(this, SIGNAL(parameterMarginC(QString)), myDatabase, SLOT(UpdateMarginSettingParameter(QString)));
    connect(this, & PLCServer::getdatapatternDataDb, myDatabase, & Database::getdatapatternDataDb);
    connect(this, & PLCServer::sortnamePattern, myDatabase, & Database::sortByName);
    connect(this, & PLCServer::sortdatePattern, myDatabase, & Database::sortByDate);
    connect(this, & PLCServer::searchByName, myDatabase, & Database::searchByName);
    connect(this, & PLCServer::searchByDate, myDatabase, & Database::searchByDate);
    connect(this, & PLCServer::getuserlogin, myDatabase, & Database::getuserlogin);

    connect(this, & PLCServer::UpdateRecipientgmail, myDatabase, & Database::UpdateRecipientgmail);
    connect(this, & PLCServer::RemoveRecipientgmail, myDatabase, & Database::RemoveRecipientgmail);
    connect(this, & PLCServer::NewRecipientgmail, myDatabase, & Database::NewRecipientgmail);
    connect(this, & PLCServer::SumNormalizationandUpdateDb, myDatabase, & Database::SumNormalizationandUpdateDb);
    connect(this, & PLCServer::deleteCsvFileAndFolder, myDatabase, & Database::deleteCsvFileAndFolder);
    connect(this, & PLCServer::getandSenddataFromCSV, this, & PLCServer::getCsvFile);
    connect(this, & PLCServer::getScreenPicture, this, & PLCServer::getScreenPictureandSave);
    connect(this, & PLCServer::uploadFile, this, & PLCServer::uploadFileWithtoftpServer);
    connect(this, SIGNAL(updateFTPParam(FTP_Param * )), myDatabase, SLOT(updateFTPParam(FTP_Param * )));
    connect(this, & PLCServer::UpdateuserNameandPassword, myDatabase, & Database::UpdateuserNameandPassword);
    connect(this, & PLCServer::RemoveuserNameandPassword, myDatabase, & Database::RemoveuserNameandPassword);
    connect(this, & PLCServer::NewuserNameandPassword, myDatabase, & Database::NewuserNameandPassword);

    connect(this, & PLCServer::updateTableDataTagging, myDatabase, & Database::updateTableDataTagging);
    connect(this, & PLCServer::deleteTableDataTagging, myDatabase, & Database::deleteTableDataTagging);
    connect(this, SIGNAL(updateMasterMode(QString, QString, QString)), myDatabase, SLOT(updateMasterMode(QString, QString, QString)));
    connect(this, & PLCServer::NewPatternFile, myDatabase, & Database::NewPatternFile);
    connect(this, & PLCServer::SavePatternFile, myDatabase, & Database::SavePatternFile);

    connect(this, &PLCServer::createSelectPatternSignal, myDatabase, &Database::createSelectPattern);
    connect(this, &PLCServer::createRangeLFLSignal, myDatabase, &Database::createRangeLFL);
    connect(this, &PLCServer::RangeLFLSignal, myDatabase, &Database::RangeLFL);
    connect(this, &PLCServer::SelectPatternSignal, myDatabase, &Database::SelectPattern);
    connect(this, &PLCServer::selectDataCSVTowerSignal, myDatabase, &Database::selectDataCSVTower);
    connect(this, &PLCServer::selectMarginSettingParameterSignal, myDatabase, &Database::selectMarginSettingParameter);
    connect(this, &PLCServer::selectMasterModeSignal, myDatabase, &Database::selectMasterModes);
    connect(this, &PLCServer::deleteOldFilesAndRecordsSignal, myDatabase, &Database::deleteOldFilesAndRecords);
    connect(this, &PLCServer::getdataMysqlSignal, myDatabase, &Database::getdataMysql);
    connect(this, &PLCServer::getSmtpParameterSignal, myDatabase, &Database::getSmtpParameter);
    connect(this, &PLCServer::getParameterEquipmentSignal, myDatabase, &Database::getParameterEquipment);
    connect(this, &PLCServer::getLFLSignal, myDatabase, &Database::getLFL);
    connect(this, &PLCServer::GetSettingDisplaySignal, myDatabase, &Database::GetSettingDisplay);
    connect(this, &PLCServer::getSettingInfoSignal, myDatabase, &Database::getSettingInfo);
    connect(this, &PLCServer::getThresholdSignal, myDatabase, &Database::getThreshold);
    connect(this, &PLCServer::getSettingNetworkSignal, myDatabase, &Database::getSettingNetwork);
    connect(this, &PLCServer::updateMarginSignal, myDatabase, &Database::updateMargin);
    connect(this, &PLCServer::gettowerAndDistanceSignal, myDatabase, &Database::gettowerAndDistance);
    connect(this, &PLCServer::getMyTaggingPhaseASignal, myDatabase, &Database::getMyTaggingPhaseA);
    connect(this, &PLCServer::getMyTaggingPhaseBSignal, myDatabase, &Database::getMyTaggingPhaseB);
    connect(this, &PLCServer::getMyTaggingPhaseCSignal, myDatabase, &Database::getMyTaggingPhaseC);
    connect(this, &PLCServer::getdatapatternDataDbSignal, myDatabase, &Database::getdatapatternDataDb);

    //    connect(this)

    // database
    connect(myDatabase, & Database::assignDisplayGerneral, this, & PLCServer::assignDisplayGerneral);
    connect(myDatabase, SIGNAL(sendUpdatedMarginList(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, & Database::newdataMysql, this, & PLCServer::newdataMysql);
    connect(myDatabase, & Database::newdataSmtpParameter, this, & PLCServer::newdataSmtpParameter);
    connect(myDatabase, & Database::newdataParameterEquipment, this, & PLCServer::newdataParameterEquipment);
    connect(myDatabase, & Database::updateNewRecipientgmail, this, & PLCServer::updateNewRecipientgmail);
    connect(myDatabase, SIGNAL(eventmsg(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(cmdmsg(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(cmdmsg(QString)), server, SLOT(sendMessageToVNC(QString)));
    connect(myDatabase, SIGNAL(updateTableDisplay(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(deletedmydatabase(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(updatedataTableA(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(updatedataTableB(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(updatedataTableC(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(updataEditDataA(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(updateThresholdA(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(updateThresholdB(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(updateThresholdC(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(UpdateSettingInfo(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(UpdatepreiodicInfo(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(packageRawData(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(cursorPosition(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(positionCursorChange(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(updatanewdistance(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(showtaggingpoint(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(sendToCal(QString)), this, SLOT(calculate(QString)));
    connect(myDatabase, & Database::uploadFile, this, & PLCServer::uploadFileWithtoftpServer);
    //    connect(myDatabase, SIGNAL(updateThresholdA(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(listOfMarginA(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(listOfMarginB(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(listOfMarginC(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(sendMarginUpdate(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(sendMessageToPLC(QString)), server, SLOT(sendMessageToMonitor(QString)));
    connect(myDatabase, SIGNAL(assignThreshold(double, double, double)), this, SLOT(assignThreshold(double, double, double)));
    connect(myDatabase, SIGNAL(assignGetSettingDisplay(double, double, double, double, double)), this, SLOT(assignGetSettingDisplay(double, double, double, double, double)));
    connect(myDatabase, SIGNAL(updateTimePeriodic(QString)), this, SLOT(updateTimePeriodic(QString)));
    connect(myDatabase, SIGNAL(updateDatePeriodic(bool, bool, bool, bool, bool, bool, bool)), this, SLOT(updateDatePeriodic(bool, bool, bool, bool, bool, bool, bool)));
    connect(myDatabase, SIGNAL(uploadToSNMP(QString, QString)), this, SLOT(uploadToSNMP(QString, QString)));
    connect(myDatabase, SIGNAL(getSettingNetworks(QString, QString, QString, QString, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool)), this, SLOT(getSettingNetworks(QString, QString, QString, QString, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool)));
    connect(myDatabase, SIGNAL(sendToWeb()), this, SLOT(sendToWeb()));
    connect(myDatabase, SIGNAL(sendMessage(QString, QWebSocket * )), this, SLOT(sendMessages(QString, QWebSocket * )));
    connect(myDatabase, SIGNAL(assignPATHCSV(QString, QString)), this, SLOT(assignPATHCSV(QString, QString)));
    connect(myDatabase, SIGNAL(startFtpTimer(QString, QString, QString)), this, SLOT(startFtpTimer(QString, QString, QString)));
    connect(myDatabase, SIGNAL(selectMasterMode(QString, QString, QString)), this, SLOT(selectMasterMode(QString, QString, QString)));
    connect(myDatabase, SIGNAL(sendToFPGA(QString)), this, SLOT(sendToFPGA(QString)));
    connect(myDatabase, SIGNAL(uploadFTP(QString)), this, SLOT(uploadFTP(QString)));
    connect(myDatabase, SIGNAL(updatePATHEmail(QString, QString)), this, SLOT(updatePATHEmail(QString, QString)));

    connect(myDatabase, SIGNAL(patternSignal(QString, QString, QString)), this, SLOT(patternSelectDelete(QString, QString, QString)));

    connect(this, &PLCServer::getEventAlarmHistory, myDatabase, &Database::getEventAlarmHistory);
    connect(this, SIGNAL(clearFpgaUpdate()), this, SLOT(clearFpgaUpdateBackground()));
    connect(this, SIGNAL(clearMonitorUpdate()), this, SLOT(clearMonitorUpdateBackground()));
    connect(this, SIGNAL(link_sdcard_web()), this, SLOT(link_sdcard_webSlot()));
    connect(this,
            &PLCServer::checkEventStorageAndEmergencyCleanupSignal,
            myDatabase,
            &Database::checkEventStorageAndEmergencyCleanup);

    qDebug() << "cppServerToMonitor start:";

    //QTimer
    connect(recheckParam, SIGNAL(timeout()), this, SLOT(recheck()));

    connect(operateTime, SIGNAL(timeout()), this, SLOT(operateTimeSlot()));
    connect(screenPictureDelayTimer, &QTimer::timeout,
            this, &PLCServer::processDelayedScreenPicture);

    qDebug() << "recheckParam start:";
    //    connect(loopTimer,SIGNAL(timeout()),this,SLOT(loopGetInfo()));
    connect(loopNetworkTimer, SIGNAL(timeout()), this, SLOT(loopNetwork()));
    qDebug() << "loopNetworkTimer start:";
    connect(patternTimer, SIGNAL(timeout()), this, SLOT(patternTimerFunction()));
    qDebug() << "patternTimer start:";
    connect(loopLogout, SIGNAL(timeout()), this, SLOT(Logout()));
    qDebug() << "loopLogout start:";
    connect(loopLogoutCounter, SIGNAL(timeout()), this, SLOT(LogoutCounter()));
    qDebug() << "loopLogoutCounter start:";
    connect(loopLogoutVNC, SIGNAL(timeout()), this, SLOT(LogoutVNC()));
    qDebug() << "loopLogoutVNC start:";
    connect(loopLogoutCounterVNC, SIGNAL(timeout()), this, SLOT(LogoutCounterVNC()));
    qDebug() << "loopLogoutCounterVNC start:";
    connect(loopWaitPic, SIGNAL(timeout()), this, SLOT(loopWaitPicSlot()));

    operateTime->setSingleShot(true);
    loopWaitPic->setSingleShot(true);
    screenPictureDelayTimer->setSingleShot(true);
    getPATHReadme();
    qDebug() << "EVENT_PATH::" << EVENT_PATH;
    //    patternTimer->start(6000);
    serverAddress = networks -> ip_timeserver;
    serverPort = 1234;
    qDebug() << "serverAddress:" << serverAddress << " serverPort:" << serverPort;
    client -> createConnection(serverAddress, serverPort);
    if (client -> isConnected == true) {
        QJsonDocument jsonDoc;
        QJsonObject Param;
        Param.insert("menuID", "getNtpPage");
        QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendToSocket(raw_data);
    }

    //    if(masterLFL == "MASTER"){
    //        plc_client->createConnection(slaveIP,5520);
    //    }
    //    else if(masterLFL == "SLAVE"){
    //        plc_client->createConnection(masterIP,5520);
    //    }
    networking -> getIPAddress("eth0");
    recheckParam -> start(1000);
    emit updateFTPParam(FTP_Param_);
    qDebug() << "recheckParam start:";
    //    loopTimer->start(200);
    qDebug() << "PLCServer";

    server -> m_MonitorSocketClients = nullptr;
    MonitorFirst_address = nullptr;
    // MonitorSecond_address = nullptr;
    vnc_address = nullptr;
    OpenPLC_address = nullptr;
    emit preiodicSetting();
    day -> times = myDatabase -> times;
    day -> Monday = myDatabase -> Monday;
    day -> Tuesday = myDatabase -> Tuesday;
    day -> Wednesday = myDatabase -> Wednesday;
    day -> Thursday = myDatabase -> Thursday;
    day -> Friday = myDatabase -> Friday;
    day -> Saturday = myDatabase -> Saturday;
    day -> Sunday = myDatabase -> Sunday;
    myDatabase -> onceTime = false;

    // myDatabase -> createSelectPattern();
    // myDatabase -> createRangeLFL();
    // myDatabase -> RangeLFL();
    // myDatabase -> SelectPattern();
    // myDatabase -> selectDataCSVTower();
    // myDatabase -> selectMarginSettingParameter();
    emit createSelectPatternSignal();
    emit createRangeLFLSignal();
    emit RangeLFLSignal();
    emit SelectPatternSignal();
    emit selectDataCSVTowerSignal();
    emit selectMarginSettingParameterSignal();

    // getCsvFile(myDatabase -> selectPatterName, "Pattern", myDatabase -> datetimefile);
    FullDistance = myDatabase -> FullDistance;
    lenghtFLF = myDatabase -> lenghtFLF;
    Distance = myDatabase -> Distance;
    TowerNo = myDatabase -> TowerNo;

    // qWarning() << "FullDistance:" << FullDistance;

    // myDatabase -> selectMasterMode();
    // myDatabase -> deleteOldFilesAndRecords();
    // myDatabase -> getdataMysql();
    // myDatabase -> getSmtpParameter();
    // myDatabase -> getParameterEquipment();
    // myDatabase -> getLFL();
    // myDatabase -> GetSettingDisplay();
    // myDatabase -> getSettingInfo();
    // myDatabase -> getThreshold();
    // myDatabase -> getSettingNetwork();
    // myDatabase -> updateMargin();

    emit selectMasterModeSignal();

    const QString protectedSelectPatterPath =
        QDir::cleanPath(myDatabase->selectPatterPath.trimmed());

    const QString protectedSelectPatterName =
        myDatabase->selectPatterName.trimmed();

    const QString protectedDateTimeFile =
        myDatabase->datetimefile.trimmed();

    qWarning() << "[PATTERN][STARTUP] Before cleanup:"
               << "selectPatterPath =" << protectedSelectPatterPath
               << "selectPatterName =" << protectedSelectPatterName
               << "datetimefile =" << protectedDateTimeFile;

    emit deleteOldFilesAndRecordsSignal();

    // Retention was already checked once during startup.  Seed the daily
    // scheduler so loopGetInfo() does not run the same cleanup again a few
    // seconds later on the same calendar day.  The next automatic run occurs
    // only when the date changes.
    keepDateTemp = QDate::currentDate().toString(QStringLiteral("dd MMM yyyy"));

    if (myDatabase->selectPatterPath.trimmed().isEmpty()
        && !protectedSelectPatterPath.isEmpty()) {

        qWarning() << "[PATTERN][GUARD] selectPatterPath was cleared by startup cleanup."
                   << "Restore =" << protectedSelectPatterPath;

        myDatabase->selectPatterPath = protectedSelectPatterPath;
    }

    if (myDatabase->selectPatterName.trimmed().isEmpty()
        && !protectedSelectPatterName.isEmpty()) {

        qWarning() << "[PATTERN][GUARD] selectPatterName was cleared by startup cleanup."
                   << "Restore =" << protectedSelectPatterName;

        myDatabase->selectPatterName = protectedSelectPatterName;
    }

    if (myDatabase->datetimefile.trimmed().isEmpty()
        && !protectedDateTimeFile.isEmpty()) {

        qWarning() << "[PATTERN][GUARD] datetimefile was cleared by startup cleanup."
                   << "Restore =" << protectedDateTimeFile;

        myDatabase->datetimefile = protectedDateTimeFile;
    }

    qWarning() << "[PATTERN][STARTUP] After cleanup:"
               << "selectPatterPath =" << myDatabase->selectPatterPath
               << "selectPatterName =" << myDatabase->selectPatterName
               << "datetimefile =" << myDatabase->datetimefile;

    emit getdataMysqlSignal();
    emit getSmtpParameterSignal();
    emit getParameterEquipmentSignal();
    emit getLFLSignal();
    emit GetSettingDisplaySignal();
    emit getSettingInfoSignal();
    emit getThresholdSignal();
    emit getSettingNetworkSignal();
    emit updateMarginSignal();

    Aux -> fails = myDatabase -> LFL_Fail;
    Aux -> operate = myDatabase -> LFL_Operate;
    * tempAux = * Aux;
    qDebug() << "let start gettowerAndDistance" << Distance.size() << TowerNo.size();
    // myDatabase -> gettowerAndDistance();
    emit gettowerAndDistanceSignal();
    myDatabase -> onceTime = true;
    qDebug() << "selectProgram" << language;
    selectProgram(language);
    //    sendFTPParam();/
    qDebug() << "thresholdA B C" << thresholdA << thresholdB << thresholdC <<
        "assignGetSettingDisplay sagFactor samplingRate distanceToStart distanceToShow fulldistance" <<
        sagFactor << samplingRate << distanceToStart << distanceToShow << fulldistance << "myDatabase->onceTime" << myDatabase -> onceTime;

    qDebug() << "getChrrentDateTime:" << getChrrentDateTime();
    QString inputPlc = "sudo systemctl stop INPUT_GPIO.service";
    system(inputPlc.toUtf8());
    QString openplc = "sudo systemctl restart OpenPLC.service";
    system(openplc.toUtf8());
    QString forward = "sudo systemctl restart PLCForward.service";
    system(forward.toUtf8());
    QString snmp = "sudo systemctl restart SNMP.service";
    system(snmp.toUtf8());
    reset_ip = false;

    if(!TowerNo.isEmpty() && !Distance.isEmpty()){
        float dis = Distance[(Distance.size() - 1)] - Distance[(Distance.size() - 2)];
        qDebug() << "distance::::" << dis << "Distance[(Distance.size()-1)]" << Distance[(Distance.size() - 1)] <<
            " Distance[10]" << Distance[10];
    }
    gps->PPS = true;

    // qWarning() << "networking -> netWorkCardAddr "<< networking -> netWorkCardAddr;
    // if (networking -> netWorkCardAddr != slaveIP) {
    //     plc_client -> createConnection(slaveIP, 5520);
    //     // qWarning() << "try to connect slaveIP" << slaveIP;
    // } else if (networking -> netWorkCardAddr != masterIP) {
    //     plc_client -> createConnection(masterIP, 5520);
    //     // qWarning() << "try to connect masterIP" << masterIP;
    // }

    // int ret = pthread_create( & idThread, NULL, ThreadFunc, this);
    // if (ret == 0) {
    //     qDebug() << ("Thread created successfully.\n");
    // } else {
    //     qDebug() << ("Thread not created.\n");
    // }

    // emit selectMarginSettingParameterSignal();

    int retnew = pthread_create( & idThreadNew, NULL, ThreadFuncNew, this);
    if (retnew == 0) {
        qDebug() << ("Thread created successfully.\n");
    } else {
        qDebug() << ("Thread not created.\n");
    }

    QTimer *memoryCleanTimer = new QTimer(this);

    connect(memoryCleanTimer, &QTimer::timeout, this, [this]() {
        const bool swapHigh = isSwapUsageHigh(128LL * 1024LL, 25);

        // clear app cache ตลอด
        // clear swap เฉพาะตอน swap เยอะ
        clearAppCacheAndSwap(true, swapHigh);
    });

    memoryCleanTimer->start(30 * 60 * 1000);
}

PLCServer::~PLCServer() {
    //    delete server;
}

void * PLCServer::ThreadFunc(void * pTr) {
    PLCServer * pThis = static_cast < PLCServer * > (pTr);
    qDebug() << "ThreadFunc1";
    pThis -> recheck();
    return NULL;
}

void * PLCServer::ThreadFuncNew(void * pTr) {
    PLCServer * pThis = static_cast < PLCServer * > (pTr);
    qDebug() << "ThreadFuncNew";
    pThis -> loopGetInfo();
    return NULL;
}


void * PLCServer::ThreadFunc2(void * pTr) {
    PLCServer * pThis = static_cast < PLCServer * > (pTr);
    qDebug() << "ThreadFunc2";
    pThis -> loopFtpTimerFunction();
    return NULL;
}

void * PLCServer::ThreadFunc3(void * pTr) {
    PLCServer * pThis = static_cast < PLCServer * > (pTr);
    qDebug() << "ThreadFunc3";
    pThis -> lftpSendFunction();
    return NULL;
}

void * PLCServer::ThreadFunc4(void * pTr) {
    PLCServer * pThis = static_cast < PLCServer * > (pTr);
    qDebug() << "ThreadFunc4";
    pThis -> LogoutVNC();
    return NULL;
}

void * PLCServer::ThreadFunc5(void * pTr) {
    PLCServer * pThis = static_cast < PLCServer * > (pTr);
    // qWarning() << "ThreadFunc5";
    pThis -> LogoutCounterVNC();
    return NULL;
}

void PLCServer::checkConnected() {
    while (1) {
        if (plc_connect < 0) {
            plc_connect = 5;
            qDebug() << "plc_connect < 0";
        }
        plc_connect--;
        qDebug() << "plc_connect:" << plc_connect;
        QThread::msleep(1500);
    }
}

void PLCServer::loopGetInfo() {
    int count=0;
    while (1) {
        if(count >= 100){
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
                qDebug() << "webapp_address size:" << webapp_address.isEmpty() << webapp_address.size();
                Q_FOREACH(QWebSocket * pClient, webapp_address) {
                    emit sendMessage(message, pClient);
                }
                if (currentDate != keepDateTemp) {
                    qDebug() << "currentDate";
                    emit calldeleteOldFilesAndRecords();
                    keepDateTemp = currentDate;
                    emit checkEventStorageAndEmergencyCleanupSignal();
                }

                // Split the time string by ':'
                QStringList timeParts = currentTime.split(':');

                if (timeParts.size() == 3) {
                    QString hh = timeParts[0]; // Hours
                    QString mm = timeParts[1]; // Minutes
                    QString ss = timeParts[2]; // Seconds

                    //                qDebug() << "Hours:" << hh;
                    //                qDebug() << "Minutes:" << mm;
                    //                qDebug() << "Seconds:" << ss;
                    QString date = QString(day -> times);
                    QStringList splitDate = date.split(".");
                    qDebug() << splitDate << "size:" << splitDate.size();
                    //                int dayTime = day->times*100;
                    int hours;
                    int minutes;
                    if (splitDate.size() > 1) {
                        hours = splitDate[0].toInt();
                        minutes = splitDate[1].toInt();
                    }
                    qDebug() << "day->times:" << day -> times << " hours:" << hours << " minutes:" << minutes <<
                        "Monday:" << day -> Monday <<
                        "Tuesday:" << day -> Tuesday <<
                        "Wednesday:" << day -> Wednesday <<
                        "Thursday:" << day -> Thursday <<
                        "Friday:" << day -> Friday <<
                        "Saturday:" << day -> Saturday <<
                        "Sunday:" << day -> Sunday;
                    if (day -> Monday == true) {
                        if (hh.toInt() == hours && mm.toInt() == minutes && ss.toInt() == 0) {
                            updateMode("Periodic");
                            QJsonDocument jsonDoc;
                            QJsonObject Param;
                            Param.insert("objectName", "Periodic");
                            QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                            emit sendMessage(raw_data, FPGA_address);

                            QJsonObject ParamPop;
                            ParamPop.insert("objectName", "eventRecord");
                            ParamPop.insert("testMode", "Periodic");
                            QString msg_pop = QJsonDocument(ParamPop).toJson(QJsonDocument::Compact).toStdString().c_str();
                            emit sendToVNC(msg_pop);
                            Q_FOREACH (QWebSocket *pClient, Monitor_address) {
                                if (pClient->state() == QAbstractSocket::ConnectedState)
                                    emit sendMessage(msg_pop, pClient);
                                else
                                    qDebug() << "Monitor_address:" << pClient->state();
                            }
                        }
                    } else if (day -> Tuesday == true) {
                        if (hh.toInt() == hours && mm.toInt() == minutes && ss.toInt() == 0) {
                            updateMode("Periodic");
                            QJsonDocument jsonDoc;
                            QJsonObject Param;
                            Param.insert("objectName", "Periodic");
                            QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                            emit sendMessage(raw_data, FPGA_address);

                            QJsonObject ParamPop;
                            ParamPop.insert("objectName", "eventRecord");
                            ParamPop.insert("testMode", "Periodic");
                            QString msg_pop = QJsonDocument(ParamPop).toJson(QJsonDocument::Compact).toStdString().c_str();
                            emit sendToVNC(msg_pop);
                            Q_FOREACH (QWebSocket *pClient, Monitor_address) {
                                if (pClient->state() == QAbstractSocket::ConnectedState)
                                    emit sendMessage(msg_pop, pClient);
                                else
                                    qDebug() << "Monitor_address:" << pClient->state();
                            }
                        }
                    } else if (day -> Wednesday == true) {
                        if (hh.toInt() == hours && mm.toInt() == minutes && ss.toInt() == 0) {
                            updateMode("Periodic");
                            QJsonDocument jsonDoc;
                            QJsonObject Param;
                            Param.insert("objectName", "Periodic");
                            QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                            emit sendMessage(raw_data, FPGA_address);

                            QJsonObject ParamPop;
                            ParamPop.insert("objectName", "eventRecord");
                            ParamPop.insert("testMode", "Periodic");
                            QString msg_pop = QJsonDocument(ParamPop).toJson(QJsonDocument::Compact).toStdString().c_str();
                            emit sendToVNC(msg_pop);
                            Q_FOREACH (QWebSocket *pClient, Monitor_address) {
                                if (pClient->state() == QAbstractSocket::ConnectedState)
                                    emit sendMessage(msg_pop, pClient);
                                else
                                    qDebug() << "Monitor_address:" << pClient->state();
                            }
                        }
                    } else if (day -> Thursday == true) {
                        if (hh.toInt() == hours && mm.toInt() == minutes && ss.toInt() == 0) {
                            updateMode("Periodic");
                            QJsonDocument jsonDoc;
                            QJsonObject Param;
                            Param.insert("objectName", "Periodic");
                            QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                            emit sendMessage(raw_data, FPGA_address);

                            QJsonObject ParamPop;
                            ParamPop.insert("objectName", "eventRecord");
                            ParamPop.insert("testMode", "Periodic");
                            QString msg_pop = QJsonDocument(ParamPop).toJson(QJsonDocument::Compact).toStdString().c_str();
                            emit sendToVNC(msg_pop);
                            Q_FOREACH (QWebSocket *pClient, Monitor_address) {
                                if (pClient->state() == QAbstractSocket::ConnectedState)
                                    emit sendMessage(msg_pop, pClient);
                                else
                                    qDebug() << "Monitor_address:" << pClient->state();
                            }
                        }
                    } else if (day -> Friday == true) {
                        if (hh.toInt() == hours && mm.toInt() == minutes && ss.toInt() == 0) {
                            updateMode("Periodic");
                            QJsonDocument jsonDoc;
                            QJsonObject Param;
                            Param.insert("objectName", "Periodic");
                            QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                            emit sendMessage(raw_data, FPGA_address);

                            QJsonObject ParamPop;
                            ParamPop.insert("objectName", "eventRecord");
                            ParamPop.insert("testMode", "Periodic");
                            QString msg_pop = QJsonDocument(ParamPop).toJson(QJsonDocument::Compact).toStdString().c_str();
                            emit sendToVNC(msg_pop);
                            Q_FOREACH (QWebSocket *pClient, Monitor_address) {
                                if (pClient->state() == QAbstractSocket::ConnectedState)
                                    emit sendMessage(msg_pop, pClient);
                                else
                                    qDebug() << "Monitor_address:" << pClient->state();
                            }
                        }
                    } else if (day -> Saturday == true) {
                        if (hh.toInt() == hours && mm.toInt() == minutes && ss.toInt() == 0) {
                            updateMode("Periodic");
                            QJsonDocument jsonDoc;
                            QJsonObject Param;
                            Param.insert("objectName", "Periodic");
                            QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                            emit sendMessage(raw_data, FPGA_address);

                            QJsonObject ParamPop;
                            ParamPop.insert("objectName", "eventRecord");
                            ParamPop.insert("testMode", "Periodic");
                            QString msg_pop = QJsonDocument(ParamPop).toJson(QJsonDocument::Compact).toStdString().c_str();
                            emit sendToVNC(msg_pop);
                            Q_FOREACH (QWebSocket *pClient, Monitor_address) {
                                if (pClient->state() == QAbstractSocket::ConnectedState)
                                    emit sendMessage(msg_pop, pClient);
                                else
                                    qDebug() << "Monitor_address:" << pClient->state();
                            }
                        }
                    } else if (day -> Sunday == true) {
                        qDebug() << "Periodic day->Sunday " << day -> Sunday;
                        if (hh.toInt() == hours && mm.toInt() == minutes && ss.toInt() == 0) {
                            updateMode("Periodic");
                            QJsonDocument jsonDoc;
                            QJsonObject Param;
                            Param.insert("objectName", "Periodic");
                            QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                            emit sendMessage(raw_data, FPGA_address);

                            QJsonObject ParamPop;
                            ParamPop.insert("objectName", "eventRecord");
                            ParamPop.insert("testMode", "Periodic");
                            QString msg_pop = QJsonDocument(ParamPop).toJson(QJsonDocument::Compact).toStdString().c_str();
                            emit sendToVNC(msg_pop);
                            Q_FOREACH (QWebSocket *pClient, Monitor_address) {
                                if (pClient->state() == QAbstractSocket::ConnectedState)
                                    emit sendMessage(msg_pop, pClient);
                                else
                                    qDebug() << "Monitor_address:" << pClient->state();
                            }
                        }
                    }
                }
                //        qDebug() << "loopGetInfo" << currentTime << QDate::currentDate().dayOfWeek();
            }

            if (OpenPLC_count > 60 * 15) {
                OpenPLC_keepAlive = false;
                if (OpenPLC_count > 60000) {
                    OpenPLC_count = 60000;
                }
            } else
                OpenPLC_keepAlive = true;

            if (snmp_count > 60 * 15) {
                snmp_keepAlive = false;
                if (snmp_count > 60000) {
                    snmp_count = 60000;
                }
            } else
                snmp_keepAlive = true;

            // // if (FPGA_count > 2 * 5) {
            // if (FPGA_count > 20) {
            //     QJsonDocument jsonDoc;
            //     QJsonObject Param;
            //     Param.insert("objectName", "reset_fpga");
            //     QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            //     if (INPUT_PLC_address -> state() == QAbstractSocket::ConnectedState) {
            //         emit sendMessage(raw_data, INPUT_PLC_address);
            //         qDebug() << "INPUT_PLC_address";
            //     } else
            //         qDebug() << "INPUT_PLC_address:" << INPUT_PLC_address -> state();
            //     FPGA_count = 0;
            //     FPGA_keepAlive = false;
            //     // }
            // } else
            //     FPGA_keepAlive = true;

            // qWarning() << "FPGA_count" << FPGA_count;
            if (FPGA_count > 60 * 15) {   // 60 * 15 = 15 นาที ถ้า loop 1s

                FPGA_keepAlive = false;

                const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                const qint64 resetFpgaIntervalMs = 3LL * 60LL * 1000LL; // 3 นาที
                // const qint64 resetFpgaIntervalMs = 20LL * 1000LL; // 3 นาที

                const bool canSendReset =
                    (m_lastResetFpgaMs == 0) ||
                    ((nowMs - m_lastResetFpgaMs) >= resetFpgaIntervalMs);

                if (canSendReset) {
                    qWarning() << "canSendReset" << canSendReset;
                    QJsonObject Param;
                    Param.insert("objectName", "reset_fpga");

                    QString raw_data = QString::fromUtf8(
                        QJsonDocument(Param).toJson(QJsonDocument::Compact)
                        );

                    if (INPUT_PLC_address &&
                        INPUT_PLC_address->state() == QAbstractSocket::ConnectedState) {

                        emit sendMessage(raw_data, INPUT_PLC_address);

                        qDebug() << "send reset_fpga to INPUT_PLC_address";

                        // จำเวลาที่ส่ง reset_fpga ล่าสุด
                        m_lastResetFpgaMs = nowMs;

                    } else {
                        qDebug() << "INPUT_PLC_address:"
                                 << (INPUT_PLC_address
                                         ? INPUT_PLC_address->state()
                                         : QAbstractSocket::UnconnectedState);
                    }
                }

                // กัน count โตเกินไป แต่ไม่ต้อง set = 60000 ทุกครั้ง
                if (FPGA_count > 60000) {
                    FPGA_count = 60000;
                }

            } else {
                FPGA_keepAlive = true;
            }

            if (plcServer_count > 60 * 15) {
            // if (plcServer_count > 10) {
                plcServer_keepAlive = false;
                OpenPLC_param -> COMMUNICATION_ERROR = !plcServer_keepAlive;
                // qWarning() << "if OpenPLC_param -> COMMUNICATION_ERROR" << OpenPLC_param -> COMMUNICATION_ERROR;
                if (plcServer_count > 60000) {
                    plcServer_count = 60000;
                }
            } else {
                plcServer_keepAlive = true;
                OpenPLC_param -> COMMUNICATION_ERROR = !plcServer_keepAlive;
                // qWarning() << "else OpenPLC_param -> COMMUNICATION_ERROR" << OpenPLC_param -> COMMUNICATION_ERROR;
            }
            // qWarning() << "plcServer_count" << plcServer_count << " plcServer_keepAlive:" << plcServer_keepAlive << " standAlone:" << standAlone;

            if (input_count > 60 * 15) {
                input_keepAlive = false;
                if (input_count > 60000) {
                    input_count = 60000;
                }
            } else
                input_count = true;

            // qDebug() << "OpenPLC_count" << OpenPLC_count << " OpenPLC_keepAlive" << OpenPLC_keepAlive;
            // qDebug() << "snmp_count" << snmp_count << " snmp_keepAlive" << snmp_keepAlive;
            // qWarning() << "FPGA_count" << FPGA_count << " FPGA_keepAlive" << FPGA_keepAlive;
            // qDebug() << "plcServer_count" << plcServer_count << " plcServer_keepAlive" << plcServer_keepAlive;
            //        qDebug() << "input_count" << input_count << " input_keepAlive" << input_keepAlive;

            OpenPLC_count++;
            snmp_count++;
            FPGA_count++;
            if(standAlone == false && masterLFL != "STANDALONE"){
                plcServer_count++;
            }
            input_count++;
            // qWarning() << "FPGA_count <<" <<  FPGA_count;
            // if (snmp_keepAlive == false && OpenPLC_keepAlive == false) {
            //     if (snmp_count > 55000 && OpenPLC_count > 55000) {
            //         exit(0);
            //     }
            // }
            count=0;
        }
        count++;
        QThread::msleep(10);
    }
}

void PLCServer::disconnected(QWebSocket * pClient) {
    qDebug() << "disconnectedpClient:" << pClient;
    // qWarning() << "disconnectedpClient:" << pClient
    //            << " Monitor_address" << Monitor_address
    //            << pClient->peerAddress() << pClient->localAddress() << pClient->peerName();
    if (pClient == OpenPLC_address) {
        //      OpenPLC_address->ignoreSslErrors();
        interlock_recheck = false;
        connectOpenPLC = false;
        qDebug() << "OpenPLC_address disconnected";
    }

    Q_FOREACH(QWebSocket * client, webapp_address) {
        if (client == pClient) {
            webapp_address.removeOne(client);
        }
    }

    // if(pClient == MonitorSecond_address){
    //     // stopThread4 = true;
    //     ipaddress_monitor = "";
    //     MonitorSecond_address = nullptr;
    //     qDebug() << "disconnected MonitorSecond_address" << MonitorSecond_address;
    // }
    // if(pClient == MonitorFirst_address){
    //     // stopThread4 = true;
    //     MonitorFirst_address = nullptr;
    //     qDebug() << "disconnected MonitorFirst_address" << MonitorFirst_address;
    // }

    //    int count=0;
    Q_FOREACH(QWebSocket * client, Monitor_address) {
        if (client == pClient) {
            // qWarning() << "Monitor_address disconnected pClient" << pClient
                       // << " client" << client;
            Monitor_address.removeAll(client);
            // qWarning() << "Monitor_address disconnected pClient" << Monitor_address;
            // server -> m_Monitor.removeAll(client);
            //            Monitor_address.removeAt(count);
        }
        if(client == vnc_address){
        }
        //        count++;
        if (client != pClient) {
            QJsonDocument jsonDoc;
            QJsonObject Param;
            Param.insert("objectName", "Pop-up");
            Param.insert("msg", "disable");
            Param.insert("state", false);
            jsonDoc.setObject(Param);
            QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
            emit sendMessage(raw_data, client);
            myDatabase->updateTaggingPoint();
            // qWarning() << "disable disconnect " << client;
        }
    }

    if(Monitor_address.size() < 1){
        change_monitor = true;
    }
    // qWarning() << "Monitor_address.size()" << Monitor_address.size()
    //            << " change_monitor:" << change_monitor;

    // qWarning() << plc_client -> isConnected
    //            << plcServer_keepAlive
    //            << plcServer_count;

    if (vnc_address == pClient) {
        stopThread5 = true;
        vnc_address = nullptr;
    }
    //    else if(pClient == snmp_address){
    //        qDebug() << "snmp_address disconnected";
    //    }
    //    else if(pClient == Monitor_address){
    //        qDebug() << "Monitor_address disconnected";
    //    }
    //    else if(pClient == FPGA_address){
    //        qDebug() << "FPGA_address disconnected";
    //    }else{
    //        qDebug() << "web application disconnected";
    //    }

}

void PLCServer::recheck()
{
    // ============================================================
    // ✅ SAFE HELPERS (กัน nullptr / pointer แขวน / state crash)
    // ============================================================
    auto wsConnected = [](const QWebSocket *ws) -> bool {
        return ws && (ws->state() == QAbstractSocket::ConnectedState);
    };

    auto safeSendOne = [&](const QString &raw, QWebSocket *ws, const char *tag) {
        if (wsConnected(ws)) {
            emit sendMessage(raw, ws);
        } else {
            qDebug() << tag << "not ready/disconnected ptr=" << ws
                     << "state=" << (ws ? ws->state() : QAbstractSocket::UnconnectedState);
        }
    };

    // ✅ ส่งให้ Monitor_address แบบปลอดภัย + ลบตัวเสียออกจาก list กันค้าง
    auto safeSendToMonitors = [&](const QString &raw) {
        for (int i = Monitor_address.size() - 1; i >= 0; --i) {
            QWebSocket *ws = Monitor_address.at(i);

            if (!wsConnected(ws)) {
                // ถ้า null หรือ unconnected ให้ remove ออก ลด pointer แขวน
                if (!ws || ws->state() == QAbstractSocket::UnconnectedState) {
                    Monitor_address.removeAt(i);
                }
                continue;
            }

            emit sendMessage(raw, ws);
        }
    };

    // ✅ helper สร้าง json compact เป็น QString แบบถูกต้อง
    auto jsonCompact = [](const QJsonObject &obj) -> QString {
        return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    };

    // ============================================================
    // ORIGINAL FLOW (แต่ทำให้ SAFE 100%)
    // ============================================================
    qDebug() << "recheck margin Monitors size:" << Monitor_address.size()
             << " empty:" << Monitor_address.isEmpty();

    if (countTostart < 60) {
        countTostart++;
        qDebug() << "countTostart < 10 countTostart:" << countTostart;
    }

    if (countTostart >= 60) {
        reconnect_count++;

        // ----------------------------
        // gps/client safe
        // ----------------------------
        if (client && client->isConnected == false) {
            if (gps) {
                gps->GPS_Data = false;
                gps->PPS = false;
            }
            qDebug() << "gps client->disConnected" << client->isConnected;
        } else {
            qDebug() << "gps isConnected" << (client ? client->isConnected : false);
        }

        // // ----------------------------
        // // update history
        // // ----------------------------
        // updateHistory++;

        // qDebug() << "[EventHistory] updateHistory =" << updateHistory
        //          << "myDatabase =" << myDatabase
        //          << "eventHistory =" << eventHistory;

        // if (updateHistory >= 10) {
        //     qDebug() << "[EventHistory] update History and alarms";

        //     if (!myDatabase) {
        //         qWarning() << "[EventHistory] myDatabase is NULL";
        //     }

        //     if (!eventHistory) {
        //         qWarning() << "[EventHistory] eventHistory is NULL";
        //     }

        //     if (myDatabase && eventHistory) {
        //         qDebug() << "[EventHistory] insert:"
        //                  << eventHistory->eventDate
        //                  << eventHistory->eventTime
        //                  << eventHistory->eventName
        //                  << eventHistory->eventStatus;

        //         myDatabase->insertEventAlarmHistory(eventHistory->eventDate,
        //                                             eventHistory->eventTime,
        //                                             eventHistory->eventName,
        //                                             eventHistory->eventStatus);

        //         eventHistory->clear();
        //     }

        //     updateHistory = 0;
        // }

        // ----------------------------
        // day compare (SAFE)
        // ----------------------------
        if (day && lastStateday && (*lastStateday != *day)) {
            qDebug() << " lastStateday:DEBUG";
            day->printinfo();
            lastStateday->printinfo();

            QJsonObject Param;
            Param.insert("objectName", "dateRemote");
            Param.insert("Monday", day->Monday);
            Param.insert("Tuesday", day->Tuesday);
            Param.insert("Wednesday", day->Wednesday);
            Param.insert("Thursday", day->Thursday);
            Param.insert("Friday", day->Friday);
            Param.insert("Saturday", day->Saturday);
            Param.insert("Sunday", day->Sunday);

            const QString raw_data = jsonCompact(Param);
            safeSendToMonitors(raw_data);
            emit sendToVNC(raw_data);

            *lastStateday = *day;
        } else {
            if (!day || !lastStateday) {
                qDebug() << "WARN: day/lastStateday null day=" << day << " lastStateday=" << lastStateday;
            }
        }

        // ----------------------------
        // keepAlive
        // ----------------------------
        {
            QJsonObject Param;
            Param.insert("objectName", "keepAlive");
            Param.insert("HwName", HwName);
            const QString raw_data = jsonCompact(Param);
            emit sendToSocketPLC(raw_data);
        }

        // ----------------------------
        // reconnect client (FIX: use &&)
        // ----------------------------
        if (client && client->isConnected == false) {
            if (!serverAddress.isEmpty() && (serverPort > 0) && (serverPort < 65536)) {
                client->createConnection(serverAddress, serverPort);
                qDebug() << "reconects gps new::";
                gpsLock = false;
            }
        }

        // getNtpPage once after connected
        if (client && client->isConnected == true) {
            if (!gpsLock) {
                QJsonObject Param;
                Param.insert("menuID", "getNtpPage");
                const QString raw_data = jsonCompact(Param);
                emit sendToSocket(raw_data);
                gpsLock = true;
                qDebug() << "sendToSocket getNtpPage::" << raw_data;
            }
        }

        // ----------------------------
        // plc_client reconnect each 10 ticks
        // ----------------------------
        if ((reconnect_count % 10) == 0) {
            if (plc_client && plc_client->isConnected == false) {
                if (networking && networking->netWorkCardAddr != slaveIP) {
                    plc_client->createConnection(slaveIP, 5520);
                } else if (networking && networking->netWorkCardAddr != masterIP) {
                    plc_client->createConnection(masterIP, 5520);
                }
                qDebug() << "isConnected:" << masterLFL;
            }
        }

        if (reconnect_count > 1000) {
            reconnect_count = 0;
        }

        // ----------------------------
        // gps compare (SAFE send)
        // ----------------------------
        if (gps) gps->print();

        {
            const QString datetime = getChrrentDateTime();
            QJsonObject Param;

            if (gps && gps->compare()) {
                OpenPLC_param->GPS_MODULE_FAIL = false;
                Param.insert("TrapsAlert", "GPS_MODULE_FAIL");
                Param.insert("state", OpenPLC_param->GPS_MODULE_FAIL);
                Param.insert("time", datetime);

                const QString raw_data = jsonCompact(Param);
                safeSendToMonitors(raw_data);
                emit sendToVNC(raw_data);
                safeSendOne(raw_data, snmp_address, "snmp_address");

                PLCserver_param->GPS_MODULE_FAIL = OpenPLC_param->GPS_MODULE_FAIL;
                if (eventHistory) eventHistory->setEvent("GPS_MODULE_FAIL", datetime, OpenPLC_param->GPS_MODULE_FAIL);
                qDebug() << "::GPS compare if ";
            } else {
                OpenPLC_param->GPS_MODULE_FAIL = true;
                Param.insert("TrapsAlert", "GPS_MODULE_FAIL");
                Param.insert("state", OpenPLC_param->GPS_MODULE_FAIL);
                Param.insert("time", datetime);

                const QString raw_data = jsonCompact(Param);
                safeSendToMonitors(raw_data);
                emit sendToVNC(raw_data);
                safeSendOne(raw_data, snmp_address, "snmp_address");

                PLCserver_param->GPS_MODULE_FAIL = OpenPLC_param->GPS_MODULE_FAIL;
                if (eventHistory) eventHistory->setEvent("GPS_MODULE_FAIL", datetime, OpenPLC_param->GPS_MODULE_FAIL);
                qDebug() << "::GPS compare else";
            }
        }

        qDebug() << "countTostart:" << countTostart;

        // ----------------------------
        // OpenPLC keepAlive / address state (SAFE)
        // ----------------------------
        if (OpenPLC_keepAlive == true) {
            connectOpenPLC = true;
            qDebug() << "OpenPLC_address:"
                     << (OpenPLC_address ? OpenPLC_address->state() : QAbstractSocket::UnconnectedState)
                     << " OpenPLC_keepAlive:" << OpenPLC_keepAlive;
        } else {
            qDebug() << "OpenPLC_address:"
                     << (OpenPLC_address ? OpenPLC_address->state() : QAbstractSocket::UnconnectedState)
                     << " address:" << OpenPLC_address;
        }

        // ----------------------------
        // COMMUNICATION_ERROR (SAFE)
        // ----------------------------
        if (OpenPLC_param->COMMUNICATION_ERROR != PLCserver_param->COMMUNICATION_ERROR) {
            const QString datetime = getChrrentDateTime();

            if (plcServer_keepAlive == true) {
                OpenPLC_param->COMMUNICATION_ERROR = false;
            } else {
                OpenPLC_param->COMMUNICATION_ERROR = true;
            }

            QJsonObject Param;
            Param.insert("TrapsAlert", "COMMUNICATION_ERROR");
            Param.insert("state", OpenPLC_param->COMMUNICATION_ERROR);
            Param.insert("time", datetime);

            const QString raw_data = jsonCompact(Param);
            safeSendToMonitors(raw_data);
            emit sendToVNC(raw_data);
            safeSendOne(raw_data, snmp_address, "snmp_address");

            // qWarning() << "send to snmp alert COMMUNICATION_ERROR state" << OpenPLC_param->COMMUNICATION_ERROR;
            if (eventHistory) eventHistory->setEvent("COMMUNICATION_ERROR", datetime, OpenPLC_param->COMMUNICATION_ERROR);

            PLCserver_param->COMMUNICATION_ERROR = OpenPLC_param->COMMUNICATION_ERROR;
        }

        // ----------------------------
        // debug states (SAFE)
        // ----------------------------
        if (wsConnected(INPUT_PLC_address))
            qDebug() << "raw_data INPUT_PLC_address";
        else
            qDebug() << "INPUT_PLC_address:" << (INPUT_PLC_address ? INPUT_PLC_address->state() : QAbstractSocket::UnconnectedState);

        if (wsConnected(snmp_address))
            qDebug() << "raw_data snmp_address";
        else
            qDebug() << "snmp_address:" << (snmp_address ? snmp_address->state() : QAbstractSocket::UnconnectedState);

        for (int i = 0; i < Monitor_address.size(); ++i) {
            QWebSocket *pClient = Monitor_address.at(i);
            if (wsConnected(pClient)){
                // qWarning() << "raw_data Monitor_address";
            }
            else
                qDebug() << "Monitor_address:" << (pClient ? pClient->state() : QAbstractSocket::UnconnectedState);
        }

        // ❗เดิมคุณ emit sendToVNC(raw_data) อีกทีแบบใช้ raw_data ตัวล่าสุด
        // เพื่อรักษาพฤติกรรมเดิม: "ไม่ส่งซ้ำแบบมั่ว" -> ไม่ส่งตรงนี้ (เพราะ raw_data ในเดิมมีโอกาสเป็นค่าจาก block ไหนก็ได้)
        // ถ้าคุณต้องการให้ส่งซ้ำจริง ๆ ให้ส่งเฉพาะ keepAlive:
        // emit sendToVNC(jsonCompact(QJsonObject{{"objectName","keepAlive"},{"HwName",HwName}}));

        // ============================================================
        // FPGA_keepAlive -> set/reset traps (SAFE)
        // ============================================================
        if (FPGA_keepAlive == true) {
            cannot_send = false;
            const QString datetime = getChrrentDateTime();

            OpenPLC_param->INTERNAL_PHASE_A_ERROR = false;
            OpenPLC_param->INTERNAL_PHASE_B_ERROR = false;
            OpenPLC_param->INTERNAL_PHASE_C_ERROR = false;
            OpenPLC_param->MODULE_HI_SPEED_PHASE_A_ERROR = false;
            OpenPLC_param->MODULE_HI_SPEED_PHASE_B_ERROR = false;
            OpenPLC_param->MODULE_HI_SPEED_PHASE_C_ERROR = false;

            PLCserver_param->INTERNAL_PHASE_A_ERROR = false;
            PLCserver_param->INTERNAL_PHASE_B_ERROR = false;
            PLCserver_param->INTERNAL_PHASE_C_ERROR = false;
            PLCserver_param->MODULE_HI_SPEED_PHASE_A_ERROR = false;
            PLCserver_param->MODULE_HI_SPEED_PHASE_B_ERROR = false;
            PLCserver_param->MODULE_HI_SPEED_PHASE_C_ERROR = false;

            auto sendTrap = [&](const QString &trapName, bool state) {
                QJsonObject Param;
                Param.insert("TrapsAlert", trapName);
                Param.insert("state", state);
                Param.insert("time", datetime);
                const QString raw = jsonCompact(Param);
                safeSendToMonitors(raw);
                emit sendToVNC(raw);
                safeSendOne(raw, snmp_address, "snmp_address");
            };

            sendTrap("INTERNAL_PHASE_A_ERROR", OpenPLC_param->INTERNAL_PHASE_A_ERROR);
            sendTrap("INTERNAL_PHASE_B_ERROR", OpenPLC_param->INTERNAL_PHASE_B_ERROR);
            sendTrap("INTERNAL_PHASE_C_ERROR", OpenPLC_param->INTERNAL_PHASE_C_ERROR);
            sendTrap("MODULE_HI_SPEED_PHASE_A_ERROR", OpenPLC_param->MODULE_HI_SPEED_PHASE_A_ERROR);
            sendTrap("MODULE_HI_SPEED_PHASE_B_ERROR", OpenPLC_param->MODULE_HI_SPEED_PHASE_B_ERROR);
            sendTrap("MODULE_HI_SPEED_PHASE_C_ERROR", OpenPLC_param->MODULE_HI_SPEED_PHASE_C_ERROR);

            if (eventHistory) {
                eventHistory->setEvent("INTERNAL_PHASE_A_ERROR", datetime, OpenPLC_param->INTERNAL_PHASE_A_ERROR);
                eventHistory->setEvent("INTERNAL_PHASE_B_ERROR", datetime, OpenPLC_param->INTERNAL_PHASE_B_ERROR);
                eventHistory->setEvent("INTERNAL_PHASE_C_ERROR", datetime, OpenPLC_param->INTERNAL_PHASE_C_ERROR);
                eventHistory->setEvent("MODULE_HI_SPEED_PHASE_A_ERROR", datetime, OpenPLC_param->MODULE_HI_SPEED_PHASE_A_ERROR);
                eventHistory->setEvent("MODULE_HI_SPEED_PHASE_B_ERROR", datetime, OpenPLC_param->MODULE_HI_SPEED_PHASE_B_ERROR);
                eventHistory->setEvent("MODULE_HI_SPEED_PHASE_C_ERROR", datetime, OpenPLC_param->MODULE_HI_SPEED_PHASE_C_ERROR);
            }

        } else {
            cannot_send = true;

            qDebug() << "FPGA_address:"
                     << (FPGA_address ? FPGA_address->state() : QAbstractSocket::UnconnectedState)
                     << "not6 alive" << FPGA_keepAlive;

            const QString datetime = getChrrentDateTime();

            OpenPLC_param->INTERNAL_PHASE_A_ERROR = true;
            OpenPLC_param->INTERNAL_PHASE_B_ERROR = true;
            OpenPLC_param->INTERNAL_PHASE_C_ERROR = true;
            OpenPLC_param->MODULE_HI_SPEED_PHASE_A_ERROR = true;
            OpenPLC_param->MODULE_HI_SPEED_PHASE_B_ERROR = true;
            OpenPLC_param->MODULE_HI_SPEED_PHASE_C_ERROR = true;

            PLCserver_param->INTERNAL_PHASE_A_ERROR = true;
            PLCserver_param->INTERNAL_PHASE_B_ERROR = true;
            PLCserver_param->INTERNAL_PHASE_C_ERROR = true;
            PLCserver_param->MODULE_HI_SPEED_PHASE_A_ERROR = true;
            PLCserver_param->MODULE_HI_SPEED_PHASE_B_ERROR = true;
            PLCserver_param->MODULE_HI_SPEED_PHASE_C_ERROR = true;

            qDebug() << "FALSE" << FPGA_keepAlive << OpenPLC_param->INTERNAL_PHASE_A_ERROR;

            auto sendTrap = [&](const QString &trapName, bool state) {
                QJsonObject Param;
                Param.insert("TrapsAlert", trapName);
                Param.insert("state", state);
                Param.insert("time", datetime);
                const QString raw = jsonCompact(Param);
                safeSendToMonitors(raw);
                emit sendToVNC(raw);
                safeSendOne(raw, snmp_address, "snmp_address");
            };

            sendTrap("INTERNAL_PHASE_A_ERROR", OpenPLC_param->INTERNAL_PHASE_A_ERROR);
            sendTrap("INTERNAL_PHASE_B_ERROR", OpenPLC_param->INTERNAL_PHASE_B_ERROR);
            sendTrap("INTERNAL_PHASE_C_ERROR", OpenPLC_param->INTERNAL_PHASE_C_ERROR);
            sendTrap("MODULE_HI_SPEED_PHASE_A_ERROR", OpenPLC_param->MODULE_HI_SPEED_PHASE_A_ERROR);
            sendTrap("MODULE_HI_SPEED_PHASE_B_ERROR", OpenPLC_param->MODULE_HI_SPEED_PHASE_B_ERROR);
            sendTrap("MODULE_HI_SPEED_PHASE_C_ERROR", OpenPLC_param->MODULE_HI_SPEED_PHASE_C_ERROR);

            if (eventHistory) {
                eventHistory->setEvent("INTERNAL_PHASE_A_ERROR", datetime, OpenPLC_param->INTERNAL_PHASE_A_ERROR);
                eventHistory->setEvent("INTERNAL_PHASE_B_ERROR", datetime, OpenPLC_param->INTERNAL_PHASE_B_ERROR);
                eventHistory->setEvent("INTERNAL_PHASE_C_ERROR", datetime, OpenPLC_param->INTERNAL_PHASE_C_ERROR);
                eventHistory->setEvent("MODULE_HI_SPEED_PHASE_A_ERROR", datetime, OpenPLC_param->MODULE_HI_SPEED_PHASE_A_ERROR);
                eventHistory->setEvent("MODULE_HI_SPEED_PHASE_B_ERROR", datetime, OpenPLC_param->MODULE_HI_SPEED_PHASE_B_ERROR);
                eventHistory->setEvent("MODULE_HI_SPEED_PHASE_C_ERROR", datetime, OpenPLC_param->MODULE_HI_SPEED_PHASE_C_ERROR);
            }
        }

        // ============================================================
        // updateEvent / fullstate block (SAFE: ทุก send ใช้ safeSend*)
        // ============================================================
        if (updateEvent == true && cannot_send == false) {
            // qWarning() << "updateEvent" << updateEvent << updateEventCount << testModeRec;
            LastNanosecRec = nanosecRec;
            LastTestModeRec = testModeRec;

            QDate date = QDate::fromString(dateRec, "yyyyMMdd");
            QTime time = QTime::fromString(timeRec, "hhmmss");
            QString currentTime = time.toString("hh:mm:ss");
            QString currentDate = date.toString("dd/MM/yyyy");

            if (updateEventCount > 2 && updateEventCount < 4) {
                QJsonObject Param;
                QJsonArray Reg;
                QString raw_datas;

                if (Aux && Aux->operate == true) {
                    // qWarning() << "Aux && Aux->operate == true:";
                    if (testModeRec == "RelayTest" || testModeRec == "Surge") {


                        if (language == 0) {
                            QJsonObject P;
                            P.insert("objectName", "writeCoilsOperate");
                            P.insert("state", true);
                            raw_datas = jsonCompact(P);
                            safeSendOne(raw_datas, INPUT_PLC_address, "INPUT_PLC_address");
                        } else if (language == 1) {
                            Reg = QJsonArray();
                            Reg.append(1);
                            QJsonObject P;
                            P.insert("objectName", "writeCoils");
                            P.insert("index", 801);
                            P.insert("register", Reg);
                            raw_datas = jsonCompact(P);
                            safeSendOne(raw_datas, OpenPLC_address, "OpenPLC_address");
                        }

                        operateTime->start(relayOperateTime);
                        qWarning() << "operateTime: start";

                        OpenPLC_param->LFL_OPERATE = true;
                        qDebug() << "3 updateMode:" << modeName << currentTime;

                        // qWarning() << "writeCoils writeCoils:" << raw_datas;

                        QJsonObject T;
                        T.insert("TrapsAlert", "LFL_OPERATE");
                        T.insert("state", OpenPLC_param->LFL_OPERATE);
                        T.insert("time", currentDate + " " + currentTime + "." + nanosecRec);
                        raw_datas = jsonCompact(T);

                        safeSendToMonitors(raw_datas);
                        emit sendToVNC(raw_datas);
                        safeSendOne(raw_datas, snmp_address, "snmp_address");

                        if (eventHistory) eventHistory->setEvent("LFL_OPERATE",
                                                   currentDate + " " + currentTime + "." + nanosecRec,
                                                   OpenPLC_param->LFL_OPERATE);
                    }
                }

                // Avoid broadcasting the same event-start trap twice.  The
                // mode-specific block below is the canonical event notification;
                // eventText is still preserved when it represents a genuinely
                // different auxiliary event message.  Normalization handles
                // variants such as "MANUAL TEST EVENT" vs "MANUAL_TEST_EVENT".
                QString canonicalModeTrap;
                if (testModeRec == "ManualTest")
                    canonicalModeTrap = QStringLiteral("MANUAL_TEST_EVENT");
                else if (testModeRec == "RelayTest")
                    canonicalModeTrap = QStringLiteral("RELAY_START_EVENT");
                else if (testModeRec == "Periodic")
                    canonicalModeTrap = QStringLiteral("PERIODIC_TEST_EVENT");
                else if (testModeRec == "PatternTest")
                    canonicalModeTrap = QStringLiteral("PATTERN_TEST_EVENT");

                auto normalizedTrapKey = [](QString value) {
                    value = value.toUpper();
                    value.remove(QRegularExpression(QStringLiteral("[^A-Z0-9]")));
                    return value;
                };

                const bool genericDuplicatesCanonical =
                    !canonicalModeTrap.isEmpty() &&
                    normalizedTrapKey(eventText) == normalizedTrapKey(canonicalModeTrap);

                if (testModeRec != "Surge" && !eventText.trimmed().isEmpty() &&
                    !genericDuplicatesCanonical) {
                    qDebug() << "eventText auxiliary notification" << eventText;

                    QJsonObject T;
                    T.insert("TrapsAlert", eventText);
                    T.insert("state", true);
                    T.insert("time", currentDate + " " + currentTime + "." + nanosecRec);
                    raw_datas = jsonCompact(T);

                    safeSendToMonitors(raw_datas);
                    emit sendToVNC(raw_datas);
                    safeSendOne(raw_datas, snmp_address, "snmp_address");
                } else if (genericDuplicatesCanonical) {
                    qWarning() << "[EVENT-NOTIFY][DUPLICATE-SUPPRESSED]"
                               << "eventText=" << eventText
                               << "canonical=" << canonicalModeTrap
                               << "mode=" << testModeRec;
                }

                qDebug() << "testModeRec" << testModeRec;

                QJsonObject T;

                if (testModeRec == "ManualTest") {
                    modeName = "Manual";
                    OpenPLC_param->MANUAL_TEST_EVENT = true;
                    T.insert("TrapsAlert", "MANUAL_TEST_EVENT");
                    T.insert("state", OpenPLC_param->MANUAL_TEST_EVENT);
                    T.insert("chanel", chanelRec);
                    if (eventHistory) eventHistory->setEvent("MANUAL_TEST_EVENT",
                                               currentDate + " " + currentTime + "." + nanosecRec,
                                               OpenPLC_param->MANUAL_TEST_EVENT);
                } else if (testModeRec == "Surge") {
                    modeName = "Surge";
                } else if (testModeRec == "RelayTest") {
                    modeName = "Relay";
                    OpenPLC_param->RELAY_START_EVENT = true;
                    T.insert("TrapsAlert", "RELAY_START_EVENT");
                    T.insert("state", OpenPLC_param->RELAY_START_EVENT);
                    T.insert("chanel", chanelRec);
                    if (eventHistory) eventHistory->setEvent("RELAY_START_EVENT",
                                               currentDate + " " + currentTime + "." + nanosecRec,
                                               OpenPLC_param->RELAY_START_EVENT);
                } else if (testModeRec == "Periodic") {
                    modeName = "Periodic";
                    OpenPLC_param->PERIODIC_TEST_EVENT = true;
                    T.insert("TrapsAlert", "PERIODIC_TEST_EVENT");
                    T.insert("state", OpenPLC_param->PERIODIC_TEST_EVENT);
                    T.insert("chanel", chanelRec);
                    if (eventHistory) eventHistory->setEvent("PERIODIC_TEST_EVENT",
                                               currentDate + " " + currentTime + ".000000000",
                                               OpenPLC_param->PERIODIC_TEST_EVENT);
                } else if (testModeRec == "PatternTest") {
                    modeName = "Pattern";
                    T.insert("TrapsAlert", "PATTERN_TEST_EVENT");
                    T.insert("state", true);
                    T.insert("chanel", chanelRec);
                    if (eventHistory) eventHistory->setEvent("PATTERN_TEST_EVENT",
                                               currentDate + " " + currentTime + "." + nanosecRec,
                                               true);
                    updateEvent = false;
                    updateEventCount = 0;
                }

                QString auditEventName;
                if (modeName == QStringLiteral("Manual"))
                    auditEventName = QStringLiteral("MANUAL_TEST_EVENT");
                else if (modeName == QStringLiteral("Relay"))
                    auditEventName = QStringLiteral("RELAY_START_EVENT");
                else if (modeName == QStringLiteral("Surge"))
                    auditEventName = QStringLiteral("SURGE_START_EVENT");
                else if (modeName == QStringLiteral("Periodic"))
                    auditEventName = QStringLiteral("PERIODIC_TEST_EVENT");
                else if (modeName == QStringLiteral("Pattern"))
                    auditEventName = QStringLiteral("PATTERN_TEST_EVENT");
                else
                    auditEventName = testModeRec;

                if (modeName != QStringLiteral("Pattern")) {
                    startEventAudit(modeName,
                                    currentDate,
                                    currentTime,
                                    auditEventName,
                                    nanosecRec + QStringLiteral("_") + chanelRec);

                    QJsonObject auditModeDetail;
                    auditModeDetail.insert(QStringLiteral("testModeRec"), testModeRec);
                    auditModeDetail.insert(QStringLiteral("modeName"), modeName);
                    auditModeDetail.insert(QStringLiteral("channel"), chanelRec);
                    auditModeDetail.insert(QStringLiteral("nanosecond"), nanosecRec);
                    auditModeDetail.insert(QStringLiteral("trapAlert"), auditEventName);
                    auditEventStep(QStringLiteral("EVENT_MODE_RESOLVED"),
                                   QStringLiteral("Resolve event mode and identity"),
                                   QStringLiteral("PASS"),
                                   QStringLiteral("Resolved event type and created a single audit/checklist context"),
                                   QStringLiteral("FPGA/OpenPLC event data"),
                                   modeName,
                                   auditModeDetail);
                }

                T.insert("time", currentDate + " " + currentTime + "." + nanosecRec);
                raw_datas = jsonCompact(T);

                // qWarning() << "Monitor_address raw_datas" << raw_datas;
                safeSendToMonitors(raw_datas);
                emit sendToVNC(raw_datas);
                safeSendOne(raw_datas, snmp_address, "snmp_address");

                if (testModeRec == "RelayTest") {
                    qDebug() << "RelayTest DEBUG PLC_DI_ERROR true" << testModeRec;
                    OpenPLC_param->PLC_DI_ERROR = true;
                    OpenPLC_param->PLC_DO_ERROR = true;
                    qDebug() << "testModeRec == RelayTest eventText" << eventText;

                    QJsonObject E1;
                    E1.insert("TrapsAlert", "PLC_DI_ERROR");
                    E1.insert("state", OpenPLC_param->PLC_DI_ERROR);
                    E1.insert("time", currentDate + " " + currentTime + "." + nanosecRec);
                    raw_datas = jsonCompact(E1);

                    if (masterLFL == "MASTER" || masterLFL == "STANDALONE") {
                        safeSendToMonitors(raw_datas);
                        emit sendToVNC(raw_datas);
                        safeSendOne(raw_datas, snmp_address, "snmp_address");
                    }

                    QJsonObject E2;
                    E2.insert("TrapsAlert", "PLC_DO_ERROR");
                    E2.insert("state", OpenPLC_param->PLC_DO_ERROR);
                    E2.insert("time", currentDate + " " + currentTime + "." + nanosecRec);
                    raw_datas = jsonCompact(E2);

                    if (masterLFL == "MASTER" || masterLFL == "STANDALONE") {
                        safeSendToMonitors(raw_datas);
                        emit sendToVNC(raw_datas);
                        safeSendOne(raw_datas, snmp_address, "snmp_address");
                        if (eventHistory) {
                            eventHistory->setEvent("PLC_DI_ERROR",
                                                   currentDate + " " + currentTime + "." + nanosecRec,
                                                   OpenPLC_param->PLC_DI_ERROR);
                            eventHistory->setEvent("PLC_DO_ERROR",
                                                   currentDate + " " + currentTime + "." + nanosecRec,
                                                   OpenPLC_param->PLC_DO_ERROR);
                    }
                    }
                }
            }

            *PLCserver_param = *OpenPLC_param;
            updateEventCount++;
        } else {
            if (updateEventCount == 0 && fullstate == false) {
                // keep original empty block
            }
        }

        qDebug() << "modeName" << modeName
                 << " updateEventCount" << updateEventCount
                 << " fullstate" << fullstate;

        // ============================================================
        // finalize event after >10 (SAFE)
        // ============================================================
        // Mandatory Event CSV + Picture now apply to Surge as well.  Do not
        // bypass fullstate for Surge: fullstate can become true only after the
        // exact local files and FTP bundle have been verified.
        if (updateEventCount > 10 && fullstate == true) {
            // qWarning() << "updateEventCount > 10 && fullstate == true" << modeName;

            updateEvent = false;
            fullstate = false;

            OpenPLC_param->PERIODIC_TEST_EVENT = false;
            OpenPLC_param->MANUAL_TEST_EVENT = false;
            OpenPLC_param->RELAY_START_EVENT = false;
            OpenPLC_param->SURGE_START_EVENT = false;
            OpenPLC_param->PLC_DI_ERROR = false;
            OpenPLC_param->PLC_DO_ERROR = false;
            OpenPLC_param->LFL_OPERATE = false;

            *PLCserver_param = *OpenPLC_param;
            updateEventCount = 0;

            const QString datetime = getChrrentDateTime();

            QJsonArray Reg;
            QString raw_datas;

            if (Aux && Aux->operate == true) {
                if (testModeRec == "RelayTest" || testModeRec == "Surge") {
                    OpenPLC_param->LFL_OPERATE = false;
                    qDebug() << "3 updateMode:" << modeName;

                    // if (language == 0) {
                    //     QJsonObject P;
                    //     P.insert("objectName", "writeCoilsOperate");
                    //     P.insert("state", false);
                    //     raw_datas = jsonCompact(P);
                    //     safeSendOne(raw_datas, INPUT_PLC_address, "INPUT_PLC_address");
                    // } else if (language == 1) {
                    //     Reg = QJsonArray();
                    //     Reg.append(0);
                    //     QJsonObject P;
                    //     P.insert("objectName", "writeCoils");
                    //     P.insert("index", 801);
                    //     P.insert("register", Reg);
                    //     raw_datas = jsonCompact(P);
                    //     safeSendOne(raw_datas, OpenPLC_address, "OpenPLC_address");
                    // }

                    QJsonObject T;
                    T.insert("TrapsAlert", "LFL_OPERATE");
                    T.insert("state", OpenPLC_param->LFL_OPERATE);
                    T.insert("time", datetime);
                    raw_datas = jsonCompact(T);

                    safeSendToMonitors(raw_datas);
                    emit sendToVNC(raw_datas);
                    safeSendOne(raw_datas, snmp_address, "snmp_address");

                    if (eventHistory) eventHistory->setEvent("LFL_OPERATE", datetime, OpenPLC_param->LFL_OPERATE);
                }
            }

            QJsonObject Param;

            if (testModeRec == "ManualTest") {
                OpenPLC_param->MANUAL_TEST_EVENT = false;
                Param.insert("TrapsAlert", "MANUAL_TEST_EVENT");
                Param.insert("state", OpenPLC_param->MANUAL_TEST_EVENT);
                qDebug() << "updateEventCount == 0 && fullstate == false testModeRec" << testModeRec;
                if (eventHistory) eventHistory->setEvent("MANUAL_TEST_EVENT", datetime, OpenPLC_param->MANUAL_TEST_EVENT);
            } else if (testModeRec == "Surge" || modeName == "Surge") {
                OpenPLC_param->SURGE_START_EVENT = false;
                Param.insert("TrapsAlert", "SURGE_START_EVENT_" + chanelRec);
                Param.insert("state", OpenPLC_param->SURGE_START_EVENT);
                if (eventHistory) eventHistory->setEvent("SURGE_START_EVENT", datetime, OpenPLC_param->SURGE_START_EVENT);
            } else if (testModeRec == "RelayTest") {
                OpenPLC_param->RELAY_START_EVENT = false;
                Param.insert("TrapsAlert", "RELAY_START_EVENT");
                Param.insert("state", OpenPLC_param->RELAY_START_EVENT);
                if (eventHistory) eventHistory->setEvent("RELAY_START_EVENT", datetime, OpenPLC_param->RELAY_START_EVENT);
            } else if (testModeRec == "Periodic") {
                OpenPLC_param->PERIODIC_TEST_EVENT = false;
                Param.insert("TrapsAlert", "PERIODIC_TEST_EVENT");
                Param.insert("state", OpenPLC_param->PERIODIC_TEST_EVENT);
                if (eventHistory) eventHistory->setEvent("PERIODIC_TEST_EVENT", datetime, OpenPLC_param->PERIODIC_TEST_EVENT);
            } else if (testModeRec == "PatternTest") {
                Param.insert("TrapsAlert", "PATTERN_TEST_EVENT");
                Param.insert("state", false);
                if (eventHistory) eventHistory->setEvent("PATTERN_TEST_EVENT", datetime, false);
            }

            // ============================================================
            // EVENT SYNC FINAL PUBLISH GATE
            // ============================================================
            // Never trust fullPicPATH/fullPathPattern directly here.  They are
            // legacy compatibility fields and may have been cleared/overwritten
            // after loopWaitPicSlot().  Re-verify mandatory physical files and
            // exact FTP transaction paths at the actual socket publish point.
            QString verifiedSyncPicPayload;
            QString verifiedSyncPatternPath;
            QString syncVerifyFailure;

            const bool syncPayloadVerified = buildVerifiedSyncPaths(
                &verifiedSyncPicPayload,
                &verifiedSyncPatternPath,
                &syncVerifyFailure);

            if (!syncPayloadVerified) {
                qWarning() << "[SYNC-PUBLISH][BLOCK] mandatory event verification failed;"
                           << "NO Monitor/VNC/SNMP SYNC notification:"
                           << syncVerifyFailure;

                QJsonObject auditDetail;
                auditDetail.insert(QStringLiteral("failureReason"), syncVerifyFailure);
                auditDetail.insert(QStringLiteral("verifiedEventBundleReady"), verifiedEventBundleReady);
                auditDetail.insert(QStringLiteral("csvLocal"), verifiedEventCsvLocalPathSnapshot);
                auditDetail.insert(QStringLiteral("picLocal"), verifiedPictureLocalPathSnapshot);
                auditDetail.insert(QStringLiteral("ftpCsv"), verifiedFtpEventCsvRemotePath);
                auditDetail.insert(QStringLiteral("ftpPic"), verifiedFtpPictureRemotePath);
                auditEventStep(QStringLiteral("SYNC_VERIFY"),
                               QStringLiteral("Final SYNC verification"),
                               QStringLiteral("FAIL"),
                               QStringLiteral("Final publish-time verification rejected mandatory Event CSV/Picture paths"),
                               verifiedEventCsvLocalPathSnapshot + QStringLiteral(" | ") + verifiedPictureLocalPathSnapshot,
                               QStringLiteral("SYNC blocked"), auditDetail);
                auditEventStep(QStringLiteral("SYNC_PUBLISH"),
                               QStringLiteral("Publish SYNC to monitor/VNC/SNMP"),
                               QStringLiteral("FAIL"),
                               QStringLiteral("SYNC notification was not sent because mandatory verification failed"),
                               QStringLiteral("PLCServer"), QStringLiteral("Monitor/VNC/SNMP"), auditDetail);
                finishEventAudit(false, QStringLiteral("Event failed at final SYNC publish verification: %1").arg(syncVerifyFailure));
                closeEventPictureOwnership(QStringLiteral("final SYNC verification failed; event closed"));

                // Explicitly destroy the legacy view so no later code can reuse
                // a stale event bundle. Pattern is optional but must also never
                // retain a previous event's path.
                fullPicPATH.clear();
                fullPathPattern.clear();
                verifiedEventBundleReady = false;
                eventBaseDateSnapshot.clear();
                eventBaseTimeSnapshot.clear();
                eventBaseModeSnapshot.clear();
            } else {
                // Publish the legacy composite PicPATH format, but build it only
                // from freshly verified REMOTE FILE paths. Pattern may be empty.
                // fullPicPATH is compatibility state only and is never trusted
                // as an input to this final publish gate.
                fullPicPATH = verifiedSyncPicPayload;
                fullPathPattern = verifiedSyncPatternPath;

                Param.insert("time", datetime);
                Param.insert("distanceA", distanceA);
                Param.insert("distanceB", distanceB);
                Param.insert("distanceC", distanceC);
                Param.insert("towerA", towerA);
                Param.insert("towerB", towerB);
                Param.insert("towerC", towerC);
                // Legacy consumer contract: PicPATH carries the three path
                // components in one string. Event CSV + Picture are verified
                // remote FTP files; Pattern is the verified LOCAL filesystem
                // source path on this PLC (or empty because Pattern is optional):
                //   EVENT <remote-csv> | PICTURE <remote-pic> | PATTERN <local-pattern-or-empty>
                Param.insert("PicPATH", verifiedSyncPicPayload);
                Param.insert("PatternPATH", verifiedSyncPatternPath);
                Param.insert("objectName", "SYNC");

                raw_datas = jsonCompact(Param);
                qWarning() << "[SYNC-PUBLISH][PASS] verified payload:" << raw_datas;

                {
                    QJsonObject auditDetail;
                    auditDetail.insert(QStringLiteral("PicPATH"), verifiedSyncPicPayload);
                    auditDetail.insert(QStringLiteral("PatternPATH"), verifiedSyncPatternPath);
                    auditDetail.insert(QStringLiteral("distanceA"), distanceA);
                    auditDetail.insert(QStringLiteral("distanceB"), distanceB);
                    auditDetail.insert(QStringLiteral("distanceC"), distanceC);
                    auditDetail.insert(QStringLiteral("towerA"), towerA);
                    auditDetail.insert(QStringLiteral("towerB"), towerB);
                    auditDetail.insert(QStringLiteral("towerC"), towerC);
                    auditDetail.insert(QStringLiteral("monitorClientCount"), Monitor_address.size());
                    auditDetail.insert(QStringLiteral("webClientCount"), webapp_address.size());
                    auditDetail.insert(QStringLiteral("snmpConnected"), snmp_address && snmp_address->state() == QAbstractSocket::ConnectedState);
                    auditEventStep(QStringLiteral("SYNC_VERIFY"),
                                   QStringLiteral("Final SYNC verification"),
                                   QStringLiteral("PASS"),
                                   QStringLiteral("Final publish-time verification passed; SYNC payload contains verified EVENT and PICTURE paths"),
                                   verifiedFtpEventCsvRemotePath + QStringLiteral(" | ") + verifiedFtpPictureRemotePath,
                                   verifiedSyncPicPayload, auditDetail);
                    auditEventStep(QStringLiteral("SYNC_PUBLISH"),
                                   QStringLiteral("Publish SYNC to monitor/VNC/SNMP"),
                                   QStringLiteral("PASS"),
                                   QStringLiteral("Publishing verified SYNC payload to all currently connected notification targets"),
                                   QStringLiteral("PLCServer"), QStringLiteral("Monitor/VNC/SNMP"), auditDetail);
                }

                safeSendToMonitors(raw_datas);
                emit sendToVNC(raw_datas);

                // Pattern is optional; this is a normal Event SYNC and mandatory
                // CSV+Picture have already passed the final gate above.
                if (testModeRec != "Pattern" && testModeRec != "PatternTest") {
                    safeSendOne(raw_datas, snmp_address, "snmp_address");
                }

                finishEventAudit(true, QStringLiteral("Event completed: mandatory files verified locally twice, FTP download-back verified, and SYNC published"));
                closeEventPictureOwnership(QStringLiteral("verified SYNC published; event Picture ownership closed"));

                // One verified event bundle is single-use for SYNC publishing.
                // Clearing the authoritative snapshots prevents a later event
                // from accidentally reusing these paths if its own Picture/CSV
                // transaction fails before producing a new bundle.
                verifiedEventBundleReady = false;
                verifiedEventCsvLocalPathSnapshot.clear();
                verifiedPictureLocalPathSnapshot.clear();
                verifiedPatternLocalPathSnapshot.clear();
                verifiedFtpEventCsvRemotePath.clear();
                verifiedFtpPictureRemotePath.clear();
                verifiedFtpPatternRemotePath.clear();
                eventBaseDateSnapshot.clear();
                eventBaseTimeSnapshot.clear();
                eventBaseModeSnapshot.clear();
            }

            if (testModeRec == "RelayTest") {
                qDebug() << "RelayTest DEBUG PLC_DI_ERROR false" << testModeRec;

                OpenPLC_param->PLC_DI_ERROR = false;
                OpenPLC_param->PLC_DO_ERROR = false;

                QJsonObject E1;
                E1.insert("TrapsAlert", "PLC_DI_ERROR");
                E1.insert("state", OpenPLC_param->PLC_DI_ERROR);
                E1.insert("time", datetime);
                raw_datas = jsonCompact(E1);

                if (masterLFL == "MASTER" || masterLFL == "STANDALONE") {
                    safeSendToMonitors(raw_datas);
                    emit sendToVNC(raw_datas);
                    safeSendOne(raw_datas, snmp_address, "snmp_address");
                }

                QJsonObject E2;
                E2.insert("TrapsAlert", "PLC_DO_ERROR");
                E2.insert("state", OpenPLC_param->PLC_DO_ERROR);
                E2.insert("time", datetime);
                raw_datas = jsonCompact(E2);

                if (masterLFL == "MASTER" || masterLFL == "STANDALONE") {
                    safeSendToMonitors(raw_datas);
                    emit sendToVNC(raw_datas);
                    safeSendOne(raw_datas, snmp_address, "snmp_address");
                }

                if (eventHistory) {
                    eventHistory->setEvent("PLC_DI_ERROR", datetime, OpenPLC_param->PLC_DI_ERROR);
                    eventHistory->setEvent("PLC_DO_ERROR", datetime, OpenPLC_param->PLC_DO_ERROR);
                }
            }

            updateEventCount = -1;
            fullPathPattern = "";
            fullPicPATH = "";
            // sendDIO = false;
        }

        if (updateEventCount == 1000) {
            updateEventCount = 8;
        }

        // ============================================================
        // Aux status changed (SAFE)
        // ============================================================
        qDebug() << "Aux->fails" << (Aux ? Aux->fails : 0) << "Aux->operate" << (Aux ? Aux->operate : 0);

        if (tempAux != Aux) {
            // statusFails
            {
                QJsonObject Param;
                Param.insert("objectName", "statusFails");
                Param.insert("LFLFAIL", Aux ? Aux->fails : 0);
                const QString raw_data = jsonCompact(Param);

                safeSendToMonitors(raw_data);
                emit sendToVNC(raw_data);
            }

            // statusOperates
            {
                QJsonObject Param;
                Param.insert("objectName", "statusOperates");
                Param.insert("LFLOPERATE", Aux ? Aux->operate : false);
                const QString raw_data = jsonCompact(Param);

                safeSendToMonitors(raw_data);
                emit sendToVNC(raw_data);
            }

            if (tempAux && Aux) {
                *tempAux = *Aux;
            }
        }

        // ============================================================
        // SYSTEM_INITIAL sync (SAFE)
        // ============================================================
        if (FPGA_keepAlive == true && snmp_keepAlive == true) {
            qDebug() << "all true FPGA_keepAlive:" << FPGA_keepAlive
                     << " OpenPLC_keepAlive:" << OpenPLC_keepAlive
                     << " snmp_keepAlive:" << snmp_keepAlive;

            const QString datetime = getChrrentDateTime();

            // objectName SYNC state true -> send to OpenPLC
            {
                QJsonObject Param;
                Param.insert("objectName", "SYNC");
                Param.insert("state", true);
                const QString raw_data = jsonCompact(Param);

                if (OpenPLC_keepAlive) {

                    // นับรอบไปเรื่อย ๆ จนครบ 30 รอบ แล้วค่อยส่ง
                    count_reset++;

                    qDebug() << "count_reset:" << count_reset << "/" << kResetThreshold
                             << " (delay sending SYNC=false)";

                    if (count_reset >= kResetThreshold) {
                        // qWarning() << "SYNC == true";
                        safeSendOne(raw_data, OpenPLC_address, "OpenPLC_address");
                        interlock_recheck = true;
                        qDebug() << "interlock_recheck (sent after threshold)";

                        // รีเซ็ตตัวนับหลังส่งแล้ว
                        count_reset = 0;
                    }

                } else {
                    // ถ้า OpenPLC หลุด keepAlive ให้รีเซ็ตตัวนับ (กันค้าง)
                    count_reset = 0;

                    qDebug() << "OpenPLC_address:"
                             << (OpenPLC_address ? OpenPLC_address->state()
                                                 : QAbstractSocket::UnconnectedState);
                }

                // if (OpenPLC_keepAlive) {
                //     qWarning() << "SYNC == true";
                //     safeSendOne(raw_data, OpenPLC_address, "OpenPLC_address");
                //     interlock_recheck = false;
                // } else {
                //     qDebug() << "OpenPLC_address:" << (OpenPLC_address ? OpenPLC_address->state() : QAbstractSocket::UnconnectedState);
                // }

                qDebug() << interlock_recheck << "interlock_recheck" << raw_data;
            }

            OpenPLC_param->SYSTEM_INITIAL = false;
            PLCserver_param->SYSTEM_INITIAL = OpenPLC_param->SYSTEM_INITIAL;

            // TrapsAlert SYSTEM_INITIAL
            {
                QJsonObject Param;
                Param.insert("TrapsAlert", "SYSTEM_INITIAL");
                Param.insert("state", OpenPLC_param->SYSTEM_INITIAL);
                Param.insert("time", datetime);

                const QString raw_data = jsonCompact(Param);
                safeSendToMonitors(raw_data);
                emit sendToVNC(raw_data);
                safeSendOne(raw_data, snmp_address, "snmp_address");

                if (eventHistory) eventHistory->setEvent("SYSTEM_INITIAL", datetime, OpenPLC_param->SYSTEM_INITIAL);
            }

        } else {
            qDebug() << "something false FPGA_keepAlive:" << FPGA_keepAlive
                     << " OpenPLC_keepAlive:" << OpenPLC_keepAlive
                     << " snmp_keepAlive:" << snmp_keepAlive;

            const QString datetime = getChrrentDateTime();
            qDebug() << "interlock_recheck" << interlock_recheck << " connectOpenPLC:" << connectOpenPLC;

            // objectName SYNC state false -> send to OpenPLC (ถ้า keepAlive)
            {
                QJsonObject Param;
                Param.insert("objectName", "SYNC");
                Param.insert("state", false);

                const QString raw_data = jsonCompact(Param);

                if (OpenPLC_keepAlive) {

                    // นับรอบไปเรื่อย ๆ จนครบ 30 รอบ แล้วค่อยส่ง
                    count_reset++;

                    qDebug() << "count_reset:" << count_reset << "/" << kResetThreshold
                             << " (delay sending SYNC=false)";

                    if (count_reset >= kResetThreshold) {
                        // qWarning() << "SYNC == false";
                        safeSendOne(raw_data, OpenPLC_address, "OpenPLC_address");
                        interlock_recheck = true;
                        qDebug() << "interlock_recheck (sent after threshold)";

                        // รีเซ็ตตัวนับหลังส่งแล้ว
                        count_reset = 0;
                    }

                } else {
                    // ถ้า OpenPLC หลุด keepAlive ให้รีเซ็ตตัวนับ (กันค้าง)
                    count_reset = 0;

                    qDebug() << "OpenPLC_address:"
                             << (OpenPLC_address ? OpenPLC_address->state()
                                                 : QAbstractSocket::UnconnectedState);
                }


                // if (OpenPLC_keepAlive) {
                //     qWarning() << "SYNC == false";
                //     safeSendOne(raw_data, OpenPLC_address, "OpenPLC_address");
                //     interlock_recheck = true;
                //     qDebug() << "interlock_recheck";
                // } else {
                //     qDebug() << "OpenPLC_address:" << (OpenPLC_address ? OpenPLC_address->state() : QAbstractSocket::UnconnectedState);
                // }

                qDebug() << interlock_recheck << "interlock_recheck" << raw_data;
            }

            OpenPLC_param->SYSTEM_INITIAL = true;
            PLCserver_param->SYSTEM_INITIAL = OpenPLC_param->SYSTEM_INITIAL;

            // TrapsAlert SYSTEM_INITIAL
            {
                QJsonObject Param;
                Param.insert("TrapsAlert", "SYSTEM_INITIAL");
                Param.insert("state", OpenPLC_param->SYSTEM_INITIAL);
                Param.insert("time", datetime);

                const QString raw_data = jsonCompact(Param);
                safeSendToMonitors(raw_data);
                emit sendToVNC(raw_data);
                safeSendOne(raw_data, snmp_address, "snmp_address");

                if (eventHistory) eventHistory->setEvent("SYSTEM_INITIAL", datetime, OpenPLC_param->SYSTEM_INITIAL);
            }
        }


        // ============================================================
        // Flush EventAlarmHistory ทุก 10 รอบ หลังจาก setEvent ทั้งหมดแล้ว
        // ============================================================
        updateHistory++;

        if (updateHistory >= 1) {
            updateHistory = 0;

            if (myDatabase && eventHistory) {
                qDebug() << "[EventHistory] insert:"
                         << eventHistory->eventDate
                         << eventHistory->eventTime
                         << eventHistory->eventName
                         << eventHistory->eventStatus;

                myDatabase->insertEventAlarmHistory(eventHistory->eventDate,
                                                    eventHistory->eventTime,
                                                    eventHistory->eventName,
                                                    eventHistory->eventStatus);

                eventHistory->clear();
            } else {
                qWarning() << "[EventHistory] cannot insert:"
                           << "myDatabase =" << myDatabase
                           << "eventHistory =" << eventHistory;
            }
        }

        // ============================================================
        // Final call
        // ============================================================
        // qWarning() << "sendSocketThreeDevice sendDIO:" << sendDIO;
        sendSocketThreeDevice();

    }
}
