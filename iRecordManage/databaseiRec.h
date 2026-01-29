#ifndef DATABASEIREC_H
#define DATABASEIREC_H

#include <QObject>
#include <QSqlDatabase>
#include <QtSql>
#include <QString>
#include <QWebSocket>
#include <QProcess>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QProcess>
#include "storagemanagement.h"
#include <QUuid>
#include <QTemporaryFile>
class DatabaseiRec : public QObject
{
    Q_OBJECT

public:
    struct DiskThreadArgs {
        DatabaseiRec* instance;
        QString msgs;
    };

    explicit DatabaseiRec(QString dbName, QString user, QString password, QString host, QObject *parent = nullptr);
    bool database_createConnection();
    bool passwordVerify(QString password);
    void genHashKey();
    void hashletPersonalize();
    bool checkHashletNotData();
    void insertNewAudioRec(QString filePath, QString radioEvent);
    void updateAudioRec(QString filePath, float avg_level, float max_level);
    bool getLastEventCheckAudio(int time, int percentFault, int lastPttMinute);
    QString getNewFile(int warnPercentFault);
    qint64 getStandbyDuration();
    void removeAudioFile(int lastMin);
    int currentFileID = 0;
    QString loadlog = "load_";
    QString filelog;
    QString logdata;
    int Serial_ID;
    bool isCheck = false;
    QString USER;
    QString IP_MASTER;
    QString IP_SLAVE;
    QString IP_SNMP;
    QString IP_TIMERSERVER;
    QString swversion;
    bool cancelFetchRecordFiles = false;
    StorageManagement *storageManager;
    QDateTime dateTimeinputLastAccess;
    QString format = "dd/MM/yyyy";
    void VerifyUserDatabase();
signals:
    void audioFault(bool fault);
    void setupinitialize(QString);
    void databaseError();
    void sendRecordFiles(QString jsonData, QWebSocket* wClient);
    void previousRecordVolume(QString);
    void cmddatabaseTomain(QString);
    void currentPathFetched(const QString &path);
    void commandMysqlToCpp(QString);
    //    void commandMysqlToWeb(QWebSocket* wClient,QString msgs);
    void commandMysqlToWeb(const QString &msg);
    void verifyUserDatabaseDone(bool ok, const QString& message);
public slots:
    void recordVolume(double,int);
    void updateRecordVolume();
    void fetchAllRecordFiles(QString msgs, QWebSocket* wClient);
    //    void fetchAllRecordFiles(QString msgs);

    void CheckAndHandleDevice(const QString& jsonString, QWebSocket* wClient);
    void RegisterDeviceToDatabase(const QString& jsonString, QWebSocket* wClient);
    void UpdateDeviceInDatabase(const QString& jsonString, QWebSocket* wClient);
    void RemoveFile(const QString& jsonString, QWebSocket* wClient);
    void getRegisterDevicePage(const QString& jsonString, QWebSocket* wClient);
    void removeRegisterDevice(const QString& jsonString, QWebSocket* wClient);
    void recordChannel(QString, QWebSocket*);
    void selectRecordChannel(QString,QWebSocket* wClient);
    void CheckandVerifyDatabases();
    void CheckandVerifyTable();
    void verifyTableSchema(const QString &tableName, const QMap<QString, QString> &expectedColumns);
    //    void selectRecordChannel(QString, QString, int, QString, QString, QWebSocket* wClient);
    static bool tableExists(QSqlDatabase& db, const QString& tableName);
    void cleanupOldRecordFiles();
    void maybeRunCleanup();
    void updatePath(const QString& jsonString, QWebSocket* wClient);
    //    void mysqlRecordDevice(const QJsonObject &obj);
    void lookupDeviceStationByIp(const QString& megs, QWebSocket* wClient);
    void recordToRecordChannel(const QJsonObject& obj);
    void formatDatabases(QString);
    void linkRecordChannelWithDeviceStation();
    void linkRecordFilesWithDeviceStationOnce();
    void nextPageOfRecorderFiles(QString megs, QWebSocket* wClient);
    void selectLocalStreaming(QString);
    void searchRecordFilesMysql(QString, QWebSocket* wClient);
    void addNewUsers(QString, QWebSocket* wClient);
    QString generateBcryptHash(const QString& password);
    void filterRecordFiles(QString, QWebSocket* wClient);
    void getUserLevel(QWebSocket* wClient);
    void editUserLevel(QString, QWebSocket* wClient);
    void deleteUserLevel(QString, QWebSocket* wClient);
    void checkFlieAndRemoveDB();
    void getCurrentPath();
    void upDateTableFileRecord();
    void deletedFileWave(const QString &jsonString, QWebSocket *wClient);
    void playRecording();

private:
    void addMissingColumn(const QString &tableName, const QString &columnName, const QString &columnType);  // ✅ เพิ่มบรรทัดนี้
    QSqlDatabase db;
    bool verifyMac();
    QString getPassword();
    qint64 getTimeDuration(QString filePath);
    void getLastEvent();
    void startProject(QString filePath, QString radioEvent);
    QString getSerial();
    QStringList getMac();
    void updateHashTable(QString mac, QString challenge, QString meta, QString serial, QString password);
    //    int deviceId;
    //    double inputFrequency;
    //    QString inputMode;
    //    QString inputIp;
    //    int inputTimeInterval;
    //    QString inputPathDirectory,inputCompanyName;
    static void* ThreadFunc( void* pTr );
    typedef void * (*THREADFUNCPTR)(void *);
    pthread_t idThread;
    QString m_lastRecordCreatedAt;
    QString m_lastRecordId;

    bool execSql(QSqlDatabase& d, const QString& sql, QString* errOut = nullptr);
    bool ensureMysqlUser(QSqlDatabase& d,
                         const QString& user,
                         const QString& host,
                         const QString& password,
                         const QString& dbName,
                         QString* errOut = nullptr);
    bool runMysqlBootstrapAsRoot(QString* errOut);
    bool tryLoginOnce(const QString& user, const QString& host, const QString& dbName, const QString& password, QString* errOut);


private slots:
    void reloadDatabase();
    //    void getEventandAlarm(QString msg);
};

#endif // DATABASEIREC_H
