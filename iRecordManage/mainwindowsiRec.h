#ifndef IRECORDMANAGE_MAINWINDOWS_H
#define IRECORDMANAGE_MAINWINDOWS_H

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QVariant>
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QImageReader>
#include <QFileInfo>
#include <QProcess>
#include <QThread>
#include <QNetworkInterface>
#include <atomic>
#include <pthread.h>
#include <iRecordManage/ChatServeriRec.h>
//#include <Databases.h>
#include <screencapture.h>
#include <iRecordManage/MAX31760.h>
#include <I2CReadWrite.h>
#include <iRecordManage/GetInputEvent.h>
#include <iRecordManage/GPIOClass.h>
#include <iRecordManage/databaseiRec.h>
#include <iRecordManage/FileDownloader.h>
#include <iRecordManage/NetworkMng.h>
#include <screencapture.h>
#include <iRecordManage/storagemanagement.h>
#include <iRecordManage/Unixsocketlistener.h>
#include <QJsonArray>
#include <QDir>
#include <cstdlib>
#include <algorithm>

#include <QtEndian>
#include <iRecordManage/max9850.h>

#include <QtEndian>
#include <iRecordManage/max9850.h>
#include "ChatServeriRec.h"
#include "ChatServerWebRec.h"
#include "ChatClientiGate.h"
#include "ChatiGateServer.h"
#include "ChatServer.h"

#define SERVER 1
#define SWVERSION "9.2.2-REC 24102025"
#define HWVERSION "iGate4CH DSP"
typedef int pj_status_t;

class mainwindowsiRec : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool recordFilesPageActive READ recordFilesPageActive
                   WRITE setRecordFilesPageActive
                       NOTIFY recordFilesPageActiveChanged)
signals:
    void cppCommand(QVariant jsonMsg);
    void sendMessage(QString);
    void captureScreenshot();
    void logLine(const QString &s);

    void requestUpdateDateTime();

    void exportProgress(int percent, const QString &status);
    void exportFinished(bool ok, const QString &outPath, const QString &error);
    void recordFileMayBeReady();
    void recordFilesPageActiveChanged(bool);
    void onSendSquelchStatus(int softPhoneID, bool pttOn, bool sqlOn, bool callState, double freq);

public:
    explicit mainwindowsiRec(QString platform, QObject *parent = nullptr);
    ~mainwindowsiRec();
    static mainwindowsiRec *instance();

    unsigned char VolumeOut = 255;
    int currentVolume = currentVolume % 64;
    int updatelevel;
    int level;
    double convertToPercent=0;
    bool psuDCInStatus = false;
    bool psuBattStatus = false;
    bool recordFilesPageActive() const { return m_recordFilesPageActive; }
public slots:
    void cppSubmitTextFiled(QString qmlJson);
    void startRuntime();
    void getDateTime();
    void checkAndUpdateRTC();
    void cppSubmitTextFiledMySQL(QString qmlJson);
    void recordDeviceLiveStream(QString,QWebSocket*);
    void deviceStatus(QString);
    void onUnixSocketMessage(const QString &msg);
    void handleRecordAction(const QString &ip,
                            const QString &freq,
                            const QString &uri);

    void handlePauseAction(const QString &ip,
                           const QString &freq,
                           const QString &uri);
    void setRecordFilesPageActive(bool active) {
        if (m_recordFilesPageActive == active) return;
        m_recordFilesPageActive = active;
        emit recordFilesPageActiveChanged(active);
    }
    void getSystemPage(QWebSocket *webSender);
    void onVerifyUserDatabaseDone(bool ok, const QString& message);
    void enableI2SLoopback();
    void ensureVoicexSymlinkAndFix();
    void onFrequencyChangedFromMain(qint64 freqHz, double freqMHz);
private:
    ChatServeriRec *SocketServer = nullptr;
    ChatServerWebRec *m_webServer = nullptr;   // << เพิ่มตัวนี้
    ChatClientiGate *newConnect;
    ChatClientiGate *dataLoggerServer;
    ChatiGateServer *SocketServeriGate;
    //    ChatServer *SocketServeriGate; ronnabaee

    struct ChatClientList
    {
        int socketIndex;
        ChatClientiGate *chatClient;
    };

    QList<ChatClientList *> chatClientList;


    bool enableDataLogger = false;
    DatabaseiRec     *mysql        = nullptr;
    MAX31760 *max31760 = nullptr;
    Max9850 *max9850;
    QWebSocket *clientSocket = nullptr;
    QTimer *storageTimer;
    NetworkMng *networking;
    QTimer *rtcUpdateTimer = nullptr;
    int port = 1234;
    QString m_dateTime;
    QTimer  m_timer;
    QTimer  m_clock;
    QWebSocket* m_currentWClient = nullptr;
    QWebSocket *wClient = nullptr;
    UnixSocketListener *m_unixReceiver = nullptr;
    uint32_t sysclkHz = 12000000;
    uint32_t sampleRate = 8000;

    bool ntp = false;
    QString timeLocation = "";
    QString ntpServer = "";
    QString SwVersion = SWVERSION;
    QString HwVersion = HWVERSION;
    int inviteMode = SERVER;

    static void* ThreadFuncDateTime(void* pTr);
    typedef void * (*THREADFUNCPTRDATETIME)(void *);
    static void* ThreadFuncFan(void* pTr);
    typedef void * (*THREADFUNCPTRFAN)(void *);
    static void* ThreadFunc( void* pTr );
    typedef void * (*THREADFUNCPTR)(void *);
    static void* ThreadFunc2( void* pTr );
    typedef void * (*THREADFUNCPTR2)(void *);
    static void* ThreadFunc3(void* pTr);
    typedef void * (*THREADFUNCPTR3)(void *);
    static void* ThreadFunc4(void* pTr);
    typedef void * (*THREADFUNCPTR4)(void *);

    pthread_t idThreaddatetime;
    pthread_t idThreadFan;
    pthread_t idThread;
    pthread_t idThread2;
    pthread_t idThread3;
    pthread_t idThread4;


    std::atomic_bool m_qmlConnected{false};
    std::atomic_bool m_threadRunning{true};
    QString date, time;
    QString fileName, filePath, fullpathFile;
    QString newMainPath;
    QJsonArray scanSdDevices() const;
    bool mountSdDevice(const QString &devName) const;   // <-- เพิ่มอันนี้
    bool unmountSdDevice(const QString &devName) const;
    bool unmountAllUsbSd() const;

    bool mergeAndConvertToMp3(const QStringList &inputs,
                              const QString     &outDir,
                              const QString     &baseName,
                              QString           &outFinalPath,
                              QString           &errorReason);
    bool interlockStatus = false;
    bool unlockStatus = false;
    bool m_recordFilesPageActive = false;
    bool serverInit = false;
    void RestartSystemServicesAfter30s();
private slots:
    void InitializingRTCtoSystem();
    void recLogging(int softPhoneID, int recorderID,QString recState, QString message);
    void VerifyFolderAndText();
    void installFfmpegIfNeeded();
};

#endif // MAINWINDOWS_H
