#ifndef ISCREENDF_H
#define ISCREENDF_H
#pragma once
// #define SwVersion "IK-3_14072025_LOG"
// #define HwVersion "Orinnx"
// #define HwName "Orinnano"

#include <QObject>
#include <QThread>
#include "QTimer"
#include "ChatClientDF.h"
#include "ChatServerDF.h"
#include "NetworkMng.h"
#include "DatabaseDF.h"
#include "DataloggerDB.h"
#include "ImageProviderDF.h"
#include "iScreenDF/TcpServerDF.h"
#include "iScreenDF/TcpClientDF.h"
#include <QSettings>
#include <QDir>
#include <QWebSocket>
#include <QQuickItem>
#include <QVector>
#include <QVariantList>
#include <QJsonArray>
#include <QProcess>
#include "newGPIOClassDF.h"
#include <QUrl>

#include <QMainWindow>
#include <QQmlContext>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantList>
#include <QSet>

#include <QFile>
#include <QTextStream>
#include <QDebug>

#include <QTcpSocket>
#include <QElapsedTimer>
#include <QNetworkInterface>

#include <iScreenDF/GpsdReader.h>
#include <iScreenDF/iClockOrin_types.h>

#include "GeographicLib/UTMUPS.hpp"
#include <GeographicLib/MGRS.hpp>

#include "CompassClient.h"

// #include "WorkerScan.h"

#define FILESETTING "/home/orinnx/.config/iSensorServer/settings.ini"

#define TP3 98

#define GPIO_SETUP_DISPLAY "gpiochip0",TP3

#define SETUP_DISPLAY_ACTIVE false
#define SETUP_DISPLAY_INACTIVE true

class iScreenDF : public QObject
{
    Q_OBJECT

public:
    explicit iScreenDF(ImageProviderDF *imageProvider, QObject *parent = nullptr);
    ~iScreenDF();

    //////////////////////////connectCompassServer ////////////////////
    void connectCompassServer(const QString &ip, quint16 port = 2948);
    void disconnectCompassServer();
    ///////////////////////////////////////////////////////////////////

    void hardwareInfo();
    static void* ThreadFunc( void* pTr );
    typedef void * (*THREADFUNCPTR)(void *);
    pthread_t idThread;

    int Display_count = 0;

    QString readLine(const QString &fileName);
    QString getUPTime();

    void sendToWeb(const QString &data);
    QList<QWebSocket *> m_Webapplication;

    void updateFirmware();
    bool foundfileupdate = false;
    QStringList findFile();
    int updateStatus = 0;
    QTimer *reConnect;

    ChatServerDF *chatServerDF;
    ChatClientDF *chartclientDF;
    TcpClientDF *localDFclient;
    TcpServerDF *tcpServerDF;
    NetworkMng *networking;
    // ImageProvider *capture;
    QString controllerName = "MainController";
    QString Serialnumber = "156952";
    QString GroupSelected = "";
    QString RemoteStatus = "";
    QString uniqueIdInGroupSelected = "";

    void ConnectKraken(const QString &ip);

    void setupNetworktoDisplay(const QString &dhcpmethod,const QString &ipaddress,const QString &subnet,const QString &getway,const QString &prids,const QString &secdns);

    QStringList acceptedNames;

    float getMemUsage();
    void setUpnetworkraken(const QString &serverKraken,const QString &iScreenip,const QString &subnet,const QString &gatway);
    // void reconnectConnectToserverKraken(const QString &ip);

    QString vfoSpectrum() const;
    QString vfoMode() const;
    int activeVfos() const;
    int outputVfo() const;
    int dspDecimation() const;
    bool optimizeShortBursts() const;

    int offset() const;

    float compassOffset() const;
    QString doaGraphType() const;
    bool spectrumPeakHold() const;
    void updateServerlogDB(const QString &ip);
    // Set values via JSON (called from C++ only)
    void maxdoaChannelvalue(const QJsonObject &obj);
    // Q_INVOKABLE void setVfoFromJson(const QJsonObject &obj);

    int NetworkIDCheck(int id);
    int ClientIndexCheck(const QString &deviceUniqueId);
    int GroupIndexCheck(const QString &uniqueIdInGroup);
    // int ClientActiveIDCheck(int roleIndex, groupActive *group);

    quint32 ipToHex(const QString &ip) const;
    // void setupServerClientForDevices(int groupId,
    //                                  const QString &groupName,
    //                                  const QJsonArray &devices);
    /////////////////////////// LOCAL REMOTE/////////////////
    // logic ภายในสำหรับ connectGroupSingle ตอน Remote พร้อมแล้ว
    void processConnectGroupSingleInternal(const QJsonObject &obj);

    // logic ฝั่งเมนู connectGroupSingle (เช็ค RemoteStatus + pending)
    void handleConnectGroupSingle(const QJsonObject &obj);

    // เก็บสถานะ connectGroupSingle ที่รอจนกว่า Remote จะเป็น REMOTE
    bool        m_pendingConnectGroup;
    QJsonObject m_pendingConnectObj;

    void setupServerClientForDevicesRemote(const QString &uniqueIdInGroup);

    void onDoAResultReceived(const QJsonObject &obj);
    void applyRfsocParameterToServer(bool needAck);
    void sendRfsocJsonLine(const QJsonObject &obj, bool addNewline);

    void updateReceiverParametersFreqOffsetBw(qint64 rfHz, double offsetHz, double bwHz);
    void updateIPServerDF();
    void broadcastMessageServerandClient(const QJsonObject &obj);
    void handleBroadcastMessage(const QJsonObject &obj);
    void updateReceiverFreqandbw(int Freq, int BW,bool link);

    void setBaseDir(const QString &dir) { m_txBaseDir = dir;}


signals:
    // EMIT  FROM cpp to /MainPage.qml
    void openPopupSettingRequested(const QString &msg);
    void setremoteGroupsJson(const QString &json);
    void setremoteDeviceListJson(const QString &json);
    void setsigGroupsInGroupSetting(const QString &json);
    void deviceFound(const QString &name,const QString &serial,const QString &ip,int ping);
    void scanFinished();
    void networkRowUpdated(const QVariantMap &row);
    void setSelectedGroupByUniqueId(const QString &uid);
    void updatecurrentFromGPSTime(const QString &GPS_DateStr, const QString &GPS_TimeStr);
    void updateLocationLatLongFromGPS(
        const QString &lat,
        const QString &lon,
        const QString &alt,
        const QString &utmText,
        const QString &mgrsText
        );
    //////////////connectCompassServer /////////////////////////////////
    // void updateDegree(double value);
    void updateDegree(const QString &serialnumber ,const QString &name, double value);
    void updateDegreelocal(double value);
    void updateStatusCompass(const QString &text);
    void recorderSettings(QString alsaDevice,QString clientIp,int frequency,QString rtspServer,QString rtspUrl,int rtspPort);
    void updateParameterMode(const QString &mode);
    void updateParameterModePopup(const QString &mode);
    void requestRemotePopup();
    void updateParameter(const QString &deviceName,const QString &serial);

    void doaFrameUpdated(const QString &serialnumber ,const QString &name,const QVariantList &thetaArray,const QVariantList &spectrumArray,double doaDeg,double confidence);
    void rfsocParameterUpdated(int frequencyHz, int doaBwHz);
    void updateGateThDbFromServer(double v);
    void rfsocDoaFftUpdated(bool doaEnable, bool fftEnable);
    void updateTxHzFromServer(double v);
    void updateDoaAlgorithmFromServer(const QString &algo);
    void updateUcaRadiusFromServer(double radiusM);
    void updateRfAgcTargetFromServer(int ch, double targetDb);
    void updateRfAgcEnableFromServer(int ch, bool enable);
    void updatelinkStatus(bool linkStatus);
    void updateServeripDfserver(const QString &ip);
    void updateGpsMarker(const QString &serialnumber ,const QString &name,double lat,double lon,double alt,const QString &dateStr,const QString &timeStr);
    void updateReceiverParametersFreqandbw(int frequencyHz, int doaBwHz , bool linkStatus);
    void updateGlobalOffsets( double offsetvalue, double compassoffset);
    void updateDoaLineMeters(int paramId);
    void updateIPLocalForRemoteGroupFromServer(const QString &ip);
    void mapOfflineChanged(bool enabled);
    void useOfflineMapStyleChanged(bool mapStatus);
    void updateDoaLineDistanceMFromServer(int m);
    void updateMaxDoaDelayMsFromServer(int ms);

public slots:
    // void cppSubmitTextFiled(QString qmlCommand);
    // void updateReceiverParameters(const QString &signalStrength, const QString &receiverGain);
    void updateNetworkSlot(const QString &dhcp,const QString &ip,const QString &subnet ,const QString &gateway,const QString &primaryDns,const QString &secondaryDns,const QString &krakenserver);
    void updateNTPServerSlot(const QString &ip ,const QString &location ,const int &method);
    void loopGetInfo();
    // void connectToserverKraken(const QString &serverKraken);
    // void getsettingDisply(const QString &status);
    // void reConnectSlot();
    // void rebootSystem(const QString &status);
    // void SendNetworkiScreentoServerKraken();
    // void SetcompassOffset(const int &val);
    // void sendpartImage(const QString &image);

    // void updateServerlogDB(const QString &ip);
    void getdataDBLogReady(const QJsonArray &array);
    void requestDataLog();


    //////////////////////////////////////////////////////
    void NetworkAppen(int id,const QString &dhcp ,const QString &ip ,const QString &subnet ,const QString &gateway ,const QString &primaryDns ,const QString &secondaryDns,const QString &phyName , const QString &krakenserver);

    // void testNetworkAppen();
    // GET FROM qml /sidepanels/SideGroup.qml
    void openPopupSetting(const QString &msg);
    void getdatabaseToSideSettingDrawer(const QString &msg);
    void groupSetting(const QString &title, int id , const QString &Title);
    void groupSettingconfig(const QString &title, int id , const QString &name, const QString &Title);
    // void scanDevices(/*const QString &baseIp = "192.168.10.",
    //                  int start = 1, int end = 254,
    //                  int port = 8000, int timeoutMs = 200*/);
    // // void scanDevices(int startHost, int endHost);
    void scanDevicesRange(const QString &startIp, const QString &endIp);
    void cancelScan();
    void getNetworkfromDb(int id);

    void updateNetworkfromDisplayIndex(int index,const QString &dhcp,const QString &ip,const QString &mask,const QString &gw,const QString &dns1,const QString &dns2);
    void restartNetworkIndex(int index);

    // void appendNewClient(int id = 0, QString name = "", QString ipAddress = "", uint16_t socketPort = 0);
    void appendNewActiveClient(const QString &deviceUniqueId,
                               const QString &uniqueIdInGroup,
                               int deviceID,
                               int groupID,
                               const QString &groupName,
                               const QString &deviceName,
                               const QString &deviceIPAddress,
                               uint16_t socketPort);
    // void closed(const QString &socketID, const QString &ip);
    void DevicesInGroupJsonReady(int groupId,
                                 const QString &groupName,
                                 const QString &groupUniqueId,
                                 const QJsonArray &devices);
    void setupServerClientForDevices(const QString &uniqueIdInGroup);

    //////////////// GPS Server /////////////////
    void gps1Updated(const GPSInfo &info);
    void gps2Updated(const GPSInfo &info);

    ////////////// connectCompassServer /////
    void Calibration(const QString &std);
    //////////// Recorder////////////////////
    void getRecorderSettings();
    void setRecorderSettings(const QString &alsaDevice,const QString &clientIp,int freq,const QString &rtspServer,const QString &rtspUrl,int rtspPort);
    //////////////// Mode //////////////////
    void setMode(const QString &mode);
    void parameterReceived(const QString &mode,const QString &deviceName,const QString &serial);
    void setParameterdevice(const QString &deviceName, const QString &serial);

    // slot get message TCP
    void onTcpMessage(const QString &message,const QHostAddress &addr,quint16 port);
    void onTcpClientConnected(const QHostAddress &addr,quint16 port);
    void onTcpClientDisconnected(const QHostAddress &addr,quint16 port);

    void ondeviceFound(QString name, QString serial, QString ip, int ping);
    void onScanFinishedBroadcast();
    //////////////////////////////////////WebSocket localDFclient ///////////////
    // void TextMessageReceived(const QString &message);
    // void SendNetworkiScreentoServerKraken(const QString &message);
    ///////////////////////////////////////////////////////////////////////////
    // void onDoAResultReceived(const QJsonObject &obj);
    void updateFromTcpServer(const QJsonObject &obj);

    void GetrfsocParameter( bool  setDoaEnable, bool spectrumEnabled, int setAdcChannel, int Frequency, int update_en, double TxHz,
                           int TargetOffsetHz, int DoaBwHz, double DoaPowerThresholdDb,const QString &DoaAlgorithm, double ucaRadiusM,double TargetDb,bool rfAgcEnabled,bool linkStatus , double offsetvalue, double compassoffset
                           ,int maxDoaLineMeters,const QString &ipLocalForRemoteGroup, int setDelayMs, int setDistance);
    void GetIPDFServer(const QString &ip);

    void sendParameterToServer();
    void updateReceiverParametersFreqandbw(int frequencyHz, int bandwidthHz);

    void sendSetDoaEnable(bool enable);
    void sendSetSpectrumEnable(bool enable);
    void sendGateThDb(double v);
    void sendTxHz(double v);
    void sendDoaAlgorithm(const QString &algo);
    void sendUcaRadiusM(double radiusM);
    void sendRfAgcTargetDb(int ch, double targetDb);
    void sendRfAgcTargetAllDb(double targetDb);
    void sendRfAgcEnable(int ch, bool enable);
    void setLinkStatus(bool linkStatus);
    void connectToDFserver(const QString &ip);
    void setCompassOffset(double offset);
    void onUpdateNetworkDfDevice(const QString &iface,
                                 const QString &dhcp,
                                 const QString &ip,
                                 const QString &mask,
                                 const QString &gw,
                                 const QString &dns1,
                                 const QString &dns2);
    void requestRfFrequency();
    void sendMaxDoaLineMeters(int meters);
    void onTxSnapshotUpdated(double lat,double lon,double rms_m,double freqHz,const QString &dateStr,const QString &timeStr,double updatedMs,const QString &mgrs);
    void setIPLocalForRemoteGroup(const QString &ip);
    void setUseOfflineMapStyle(bool mapStatus);
    void setDelayMs(const int ms);
    void setDistance(const int m);

private slots:
    void remoteGroupsJson(const QString &json);
    void remoteSideRemoteJson(const QString &json);
    void sigGroupsInGroupSetting(const QString &json);

    void socketClientClosed(int socketID, const QString &ip);
    void onDeviceConnected(const QString &uniqueIdInGroup, const QString &ipaddress);
    void TextMessageReceivedFromClient(const QString &message);

    //////////////connectCompassServer /////////////////////////////////
    void onCompassConnected();
    void onCompassDisconnected();
    void onCompassError(const QString &err);
    void onCompassHeadingUpdated(double heading);
    void calibStatusChanged(const QString &mode,const QString &state,const QString &rotate,double progressDeg,bool done,const QString &instruction);
    //////////// Recorder////////////////////
    void recorderSettingsReady(QString alsaDevice,QString clientIp,int frequency,QString rtspServer,QString rtspUrl,int rtspPort);


private:
    bool m_blockUiSync = false;
    QString localIpAddress() const;

    ChatClientDF *chatClient = nullptr;
    QThread    *dbThread;
    QAtomicInteger<bool> m_scanning;
    float m_memUsage = 0.0f;

    int count = 0;
    double memUsage = 0;

    QWebSocket m_webSocket;

    struct Krakenparameter
    {
        QString ServerKraken;
    };
    Krakenparameter *krakenparameter;

    struct Network{
        // network
        QString dhcpmethod;
        QString ip_address = "127.0.0.1";
        QString subnet;
        QString ip_gateway = "";
        QString pridns;
        QString secdns;
        QString phyName = "bond0";
        QString krakenserver = "";

        //location
        QString location = 0;

        //ntp server
        QString ip_timeserver = "";
        int method_timeserver = 0;
    };

    struct NetworkServerKraken{
        // network
        QString dhcpmethod;
        QString ip_address = "127.0.0.1";
        QString subnet;
        QString ip_gateway = "";
        QString pridns;
        QString secdns;
        QString phyName = "eth0";
    };

    struct Network2 {
        int id;
        QString dhcpmethod;
        QString ip_address = "127.0.0.1";
        QString subnet;
        QString ip_gateway = "";
        QString pridns;
        QString secdns;
        QString phyName = "bond0";
        QString krakenserver = "";
    };
    struct clientNode
    {
        QString deviceUniqueId;    // แทน deviceIndex
        QString uniqueIdInGroup;   // แทน groupIndex
        int deviceID = 0;          // ID ใน database DeviceGroups
        int groupID = 0;           // ID ของ group
        QString devicename = "";
        QString ipAddress = "";
        uint16_t socketPort = 0;
        ChatClientDF *chatclient = nullptr;

        int clientCountGetInfo = 0;
        int timeout = 1;
        QDateTime lastUpdate;
        QJsonObject lastMessage;
        double latitude = 0.0;
        double longitude = 0.0;
        bool Connected = false;
        QString descriptions = "";
        int status = 0;
    };
    struct groupActive
    {
        QString uniqueIdInGroup;
        int userID = 0;
        int groupID = 0;
        QString groupName = "";
        QList<clientNode *> client_active_list;
    };


    struct Parameter
    {
        bool m_setDoaEnable = 1;
        bool m_spectrumEnabled = 1;
        int m_setAdcChannel = 0;
        int m_Frequency = 120000000;
        int m_update_en = 31;
        float m_txHz = 10;
        int m_TargetOffsetHz = 50000;
        int m_doaBwHz = 12000;
        float m_doaPowerThresholdDb = - 65.1;
        QString m_doaAlgorithm = "uca_rb_music";
        double m_scannerAttDb = 0.0;
        double m_ucaRadiusM = 0.8;
        bool   m_rfAgcEnabled = false;
        bool   m_rfAgcChEnabled[5] = { true, true, true, true, true };
        double m_rfAgcTargetDb[5]  = { -70.0, -70.0, -70.0, -70.0, -70.0 };
        bool m_linkStatus = false;
        QString m_ipdfServer = "192.168.10.78";
        double m_offset_value = 0.0;
        double m_compass_offset = 0.0;
        int m_maxDoaLine_meters = 0.0;
        QString m_ipLocalForRemoteGroup = "10.10.0.20";
    };

    Network *networks;
    NetworkServerKraken *netServerKraken;
    DatabaseDF *db;
    DataloggerDB *logdb;

    QString lastGetCurrentTime = "";
    QList<QWebSocket *> m_clients;

    QList<Network2*> m_network2List;

    QList<Parameter*> m_parameter;

    QList<clientNode *> client_list;
    QList<groupActive *> group_active_list;

    int ClientActiveIDCheck(const QString &uniqueIdInGroup, groupActive *group);

    void gpioInit();
    newGPIOClassDF *displaysetting = nullptr;
    ImageProviderDF *capture;

    QVector<ChatClientDF*> m_groupClients;
    bool m_groupShuttingDown = false;
    void closeAllGroupClients();
    void closeGroupClientsByIp(const QString &targetIp);
    QTimer *keepAliveTimer;
    QTimer *compassTimer;
    ////////////////////////////////GPS////////////////////////
    GpsdReader        *gpsReader       = nullptr;
    // state GPS
    GPSInfo            state1_;
    GPSInfo            state2_;
    bool               best_pps    = true;
    QString makeGpsJson(const QString& port, const GPSInfo& info) const;
    void updatePpsCtl();

    //////////////connectCompassServer /////////////////////////////////
    CompassClient *m_compassClient = nullptr;

private:
    QString m_imgBaseDir = "/var/www/html/image";

    // core
    bool deleteImageRel(const QString &rel, QString *reasonOut);
    void pruneEmptyDirs(const QString &absFilePath);
    void sendReloadWeb();

    QString m_txBaseDir = "/var/www/html/log/iScreenDF";

    QString m_lastTxSeenKey;
    QString m_lastTxWrittenKey;
    // =========================
    // ACTIVE FILE (per freq+day)
    // =========================
    QString m_activeFreqFolder;   // เช่น "120_000MHz"
    QString m_activeDayFolder;    // เช่น "2026-01-22"
    QString m_activeCsvPath;      // full path ของไฟล์ที่กำลัง append อยู่

    // =========================
    // HELPERS
    // =========================
    static QString sanitizePathPart(QString s);

    static QString freqToFolder(double freqHz);                        // "120_000MHz"
    static QString dateToFolder(const QString &dateStr, double ms);    // "2026-01-22"
    static QString timeToFolder(const QString &timeStr, double ms);    // "14-12-49" (ยังใช้ในแถว CSV ได้)

    QString buildDailyCsvPath(double freqHz,
                              const QString &dateStr,
                              double updatedMs) const;
    void updateActiveCsvIfNeeded(double freqHz,
                                 const QString &dateStr,
                                 double updatedMs);

    static bool ensureDir(const QString &dirPath);

    static bool appendTxCsvRow(const QString &filePath,
                               const QString &latStr,
                               const QString &lonStr,
                               const QString &rmsStr,
                               const QString &freqStr,
                               const QString &dateStr,
                               const QString &timeStr,
                               const QString &updatedMsStr,
                               const QString &mgrs);
    bool deleteRel(const QString &rel, QString *reasonOut);
    void cleanupEmptyDirs(const QString &absFilePath);
    void sendResult(const QJsonObject &o);

private slots:
    void newCommandProcess(const QJsonObject &command, QWebSocket *pSender,const QString &message);
    // void TextMessageReceived(const QString &message);
    // void socketClientReconnect();
    // void TextMessageReceivedFromClient(const QString &message);
};

#endif // MAINWINDOWS_H
