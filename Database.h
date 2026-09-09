#ifndef DATABASE_H
#define DATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QtSql>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include "QWebSocketServer"
#include "QWebSocket"
#include <QVector>
#include <algorithm>
#include <QStorageInfo>
class Database :  public QObject
{
Q_OBJECT
public:
   explicit Database(QString dbName, QString user, QString password, QString host, QObject *parent = nullptr);
         ~Database();
   void insertCSVTower();
   bool database_createConnection();
   void restartMysql();
   bool upgradeDatabase();
   void autoBackupTimerStart();
   bool addColumnDataTagging();
   QString dateStr = "";
   QString timeStr = "";
   QString EVENT_PATH = "/mnt/sdcard/event_records/";
   QString TOWER_NO = "/mnt/sdcard/tower/";
   QString PIC_PATH = "/mnt/sdcard/event_records/Pic";
   QString MANUAL_PATH = "/mnt/sdcard/event_records/Manual";
   QString RELAY_PATH = "/mnt/sdcard/event_records/Relay";
   QString SURGE_PATH = "/mnt/sdcard/event_records/Surge";
   QString PATTERN_PATH = "/mnt/sdcard/event_records/Pattern";
   QString PERIODIC_PATH = "/mnt/sdcard/event_records/Periodic";
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
   void updateSettingNetwork(QString ,QString, QString, QString, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool);
   bool LFL_Fail;
   bool LFL_Operate;
   bool Monday, Tuesday, Wednesday, Thursday, Friday, Saturday, Sunday;
   QString times = "";
   int MaxMarginA = 0, MaxMarginB = 0, MaxMarginC = 0;
   int lenghtFLF = 0;
   int lenghtMarginA = 0,lenghtMarginB = 0,lenghtMarginC = 0;
   int valueOfMarginA[100],valueOfMarginB[100],valueOfMarginC[100];
   QJsonArray distAPat, voltAPat, distBPat, voltBPat, distCPat, voltCPat;

   void patternDataDb(QSqlQuery query);

   int getCategoryId(const QString &category);
   struct TowerAndDistance{
            QString substation;
            QString direction;
            QString linenumber;
   };
   QString fileNames = "";
   TowerAndDistance *towerAndDistance;
   bool onceTime = false;
   int getUserLevel(QString username, QString password);

   struct SetupParameterEquipment{
       QString SubstationName;
       int Voltage;
       QString TransmissionLineName;
       QString Distance;
       QString IPaddress;
       QString Brand;
       QString Model;
       QString SerialNo;
       QString ContractNumber;
       QString Date;
       QString LFLSerialNo;
   };
   SetupParameterEquipment *SetupEquipment;
   struct FTP_Param{
       QString FTP_IP = "";
       QString USERNAME = "";
       QString PASSWORD = "";
       QString PERIODIC_FILE = "";
       QString RELAY_FILE = "";
       QString SURGE_FILE = "";
       QString MANUAL_FILE = "";
       QString PATTERN_FILE = "";

       void printinfo(){
           qDebug() << "database printinfo FTP_Param FTP_IP:" << FTP_IP
                    << " USERNAME:" << USERNAME << " PASSWORD:" << PASSWORD
                    << " PERIODIC_FILE:" << PERIODIC_FILE << " RELAY_FILE:" << RELAY_FILE
                    << " SURGE_FILE:" << SURGE_FILE << " MANUAL_FILE:" << MANUAL_FILE
                    << " PATTERN_FILE:" << PATTERN_FILE;
       }
   };

   FTP_Param *FTP_Param_;
    QString TimeKept,DateKept;
    QString TransmissionLine;
    float FullDistance;
    QList<float> Distance;
    QStringList TowerNo;
    QString selectPatterPath="";
    QString selectPatterName="";
    QString datetimefile="";
    void updateSelectPattern(QString,QString,QString);
    void updateRangeLFL(QString);
    QString fullPathPattern="";
    QString fullPicPATH="";
    bool ensureEventAlarmHistoryTimeColumn();
    QString normalizeTimeForMariaDbTime6(const QString &input);

signals:
    void assignDisplayGerneral(double,QString,QString,QString);
    void patternSignal(QString,QString);
    void patternSignal(QString,QString,QString);
    void updatePATHEmail(QString,QString);
    void uploadFTP(QString);
    void sendToFPGA(QString);
    void selectMasterMode(QString,QString,QString);
    void startFtpTimer(QString,QString, QString);
    void assignPATHCSV(QString,QString);
    void sendToWeb();
    void createDirectoryIfNeeded(QUrl ftpUrl, QString directoryPath, QString username, QString password);
   void getSettingNetworks(QString ,QString, QString, QString, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool);
   void uploadToSNMP(QString,QString);
   void updateDatePeriodic(bool,bool,bool,bool,bool,bool,bool);
   void updateTimePeriodic(QString);
   void assignThreshold(double,double,double);
   void assignGetSettingDisplay(double,double,double,double,double);
    void mysqlError();
    void newdataMysql(QString ip,bool plcDO,bool plcDI,bool HiSpeedPhaseA,bool HiSpeedPhaseB,bool HiSpeedPhaseC, bool modbusPhaseA, bool modbusPhaseB, bool modbusPhaseC,
                      bool gpsModule, bool systeminit, bool communication, bool relayStart, bool surgeStart, bool periodic, bool manualTest, bool lflFail, bool lflOperate);
    void newdataSmtpParameter(QString senderEmail,QString NamesenderEmail,QString Password,QString recipientEmail,QString recipientName,QString SmtpServer,int SmtpPort);
    void newdataParameterEquipment( QString SubstationName,int Voltage,QString TransmissionLineName,QString Distance,QString IPaddress,QString Brand,QString Model,QString SerialNo,
                                   QString ContractNumber,QString Date,QString LFLSerialNo);
    void audioFault(bool fault);
    void setupinitialize(QString);

    void databaseError();
    void eventmsg(QString);
    void cmdmsg(QString);
    void sendMessage(QString, QWebSocket *);
    void deletedmydatabase(QString);
    void updateTableDisplay(QString);
    void updatedataTableA(QString message);
    void updatedataTableB(QString message);
    void updatedataTableC(QString message);
    void updataEditDataA(QString);
    void listOfMarginA(QString);
    void listOfMarginB(QString);
    void listOfMarginC(QString);
    void updateThresholdA(QString);
    void updateThresholdB(QString);
    void updateThresholdC(QString);
    void UpdateSettingInfo(QString);
    void UpdatepreiodicInfo(QString);
    void packageRawData(QString);
    void cursorPosition(QString);
    void positionCursorChange(QString);
    void updatanewdistance(QString);
    void showtaggingpoint(QString);
    void sendToCal(QString);
    void updateNewRecipientgmail(QString message);

    void uploadFile(QString filePath,QString ftpUrl,QString username,QString password);
    void sendMarginUpdate(QString);
    void sendMessageToPLC(QString);
    void sendUpdatedMarginList(QString);

public slots:
    void updateSettingGeneralInfo(const QString &msg);
    void checkEventStorageAndEmergencyCleanup();
    void setNTPServer(QString);
    void writeMarginCSV(QString path,QString countMarginA, QString countMarginB, QString countMarginC, const QJsonArray& marginAItems, const QJsonArray& marginBItems, const QJsonArray& marginCItems);

    //slot new
    bool gettowerAndDistance();
    void selectMasterModes();
    void getSettingNetwork();
    void getLFL();
    bool getParameterEquipment();
    bool getdataMysql();
    bool getSmtpParameter();
    void selectDataCSVTower();
    void SelectPattern();
    void deleteSelectPattern();
    void RangeLFL();
    void createRangeLFL();
    void createSelectPattern();

    // slot old
    void selectMarginSettingParameter();
    void updateSetupEquipment(SetupParameterEquipment *);
    void deleteOldFilesAndRecords();
    void updateMasterMode(const QString &newUser, const QString &newIpMaster, const QString &newIpSlave);
    void updatePathNotMount(QString,QString);
    void uploadCSVTower();
    void updateFTPParam(FTP_Param *);
    void ToShowSettingInfo(QString);
    void updateSettingInfo(QString);
    bool updateSnmpipserver(QString ip);
    bool updateSnmpSelectionState(bool plcDO,bool plcDI,bool HiSpeedPhaseA,bool HiSpeedPhaseB,bool HiSpeedPhaseC, bool modbusPhaseA, bool modbusPhaseB, bool modbusPhaseC,
                                   bool gpsModule, bool systeminit, bool communication, bool relayStart, bool surgeStart, bool periodic, bool manualTest, bool lflFail, bool lflOperate);
    bool insertIntoSnmpTrepDataLogger(const QString& alertTrap, const QString& alertState);
    void insertEventAlarmHistory(QStringList,QStringList,QStringList,QStringList);
    void getEventAlarmHistory();
    bool updateEmail_Config(QString senderEmail,QString senderName,QString password,QString recipientEmail,QString recipientName,QString Smtpserver,int SmtpPort);
    bool updateParameterEquipment( QString SubstationName,int Voltage,QString TransmissionLineName,QString Distance,QString IPaddress,QString Brand,QString Model,QString SerialNo,
                                   QString ContractNumber,QString Date,QString LFLSerialNo);
    void cleanDataInGraph(QString);
    void getEventandAlarm(QString msg);
    void DistanceandDetailPhaseA(QString msg);
    void DistanceandDetailPhaseB(QString msg);
    void DistanceandDetailPhaseC(QString msg);
    void getMySqlPhaseA(QString msg);
    void getMySqlPhaseB(QString msg);
    void getMySqlPhaseC(QString msg);
    void deletedDataMySQLPhaseA(QString msg);
    void deletedDataMySQLPhaseB(QString msg);
    void deletedDataMySQLPhaseC(QString msg);
    void updateDataBaseDisplay(QString msg);
    void updateTablePhaseA(QString);
    void updateTablePhaseB(QString);
    void updateTablePhaseC(QString);
    void UpdateMarginSettingParameter(QString);
    void UpdateMarginSettingParameter(int margin,
                                                double valueVoltage,
                                                int focusIndex,
                                      const QString &phase);
    void edittingMysqlA(QString);
    void edittingMysqlB(QString);
    void edittingMysqlC(QString);
    void closeMySQL();
    void updateTableDataTagging(int No , double Distance , QString  Detail);


    void configParemeterThreshold(QString);

    void getThreshold();

    void getSettingInfo();

    void getpreiodicInfo();

    void getUpdatePeriodic(QString);

    void getUpdateWeekly(QString);

    void userMode(QString);
    void getUpdateUserMode();

    void storeStatusAux(QString);
    void getPositionDistance(QString);

    void controlCursor(QString);
    void getChangeDistance(QString);
    void updateDistance(double);
    void taggingpoint(QString);
    void updataStatusTagging(int,bool);
    void SettingDisplay(QString);
    void GetSettingDisplay();
    void GetSag();
    void getMyTaggingPhaseA();
    void getMyTaggingPhaseB();
    void getMyTaggingPhaseC();
    void updateTaggingPoint(QString msg);
    void updateTaggingPoint();
    void fetchTaggingData();
    ///-------------get datastorag from DB -------------------------///
    /// \brief db
    void getdatapatternDataDb();
    void sortByName(bool ascending,const QString &categoryName);
    void sortByDate(bool descending,const QString &categoryDate);
    void searchByName(const QString &name, const QString &categoryName);
    void searchByDate(const QString &date, const QString &categoryDate);
//--------------------Recipientgmail-----------------------------//
    void getrecipientEmail();
    void UpdateRecipientgmail(QString gmail, int id);
    void RemoveRecipientgmail(QString gmail);
    void NewRecipientgmail(QString gmail);


    //---------------------userNameandPassword--------------------------///
    /// \brief updateMargin
    void UpdateuserNameandPassword(QString UserName,QString Password,int UserLevel,int id);
    void RemoveuserNameandPassword(QString UserName,QString Password,int UserLevel);
    void NewuserNameandPassword(QString UserName,QString Password,int UserLevel);

    void updateMargin();
    void updataListOfMarginA(QString);
    void updataListOfMarginB(QString);
    void updataListOfMarginC(QString);

    void configParemeterMarginA(QString);
    void configParemeterMarginB(QString);
    void configParemeterMarginC(QString);
    void scanFiles();
    void deleteCsvFileAndFolder(QString fileName,QString category, QString date);
    void renameFileAndUpdateDb(const QString &oldFileName, const QString &newFileName, const QString &category, const QDate &date);
//    void getCsvFile(QString fileName, QString category, QString date);
//    void updateQuery(const QString &queryStr);
//    void searchByName(const QString &name);
//    void searchByDate(const QString &date);
//    void sortByName(bool ascending);
//    void sortByDate(bool descending);
//    void RecalculateWithMargin(QString msg);
    void SumNormalizationandUpdateDb(QString modeName,QString phase,QString FileTimeStamp,QJsonArray dist,QJsonArray volt);
    void getuserlogin(QString username , QString password, QWebSocket *);
    void deleteTableDataTagging(int No);
    void NewPatternFile(QString modeName ,QString Name);
    void SavePatternFile(QString modeName, QString Name ,QString event_datetime);
    void rewritePattern(QJsonArray,QJsonArray,QJsonArray,QJsonArray,QJsonArray,QJsonArray);


    void updataListOfMarginANotObject(int no, int valueOfMargin);
    void updataListOfMarginBNotObject(int no, int valueOfMargin);
    void updataListOfMarginCNotObject(int no, int valueOfMargin);

    void updateMarginSettingParameter(int margin, int valueVoltage, int focusIndex, QString phase);

    void resetAllMarginTablesValueToZero();

private:
    void writeCSVSinglePhase(const QString& fileName, const QString& FullName, const QJsonArray& dist, const QJsonArray& volt);
    void writeCSV(QString,const QString& fileName,const QString& FullName, const QJsonArray& distA, const QJsonArray& voltA, const QJsonArray& distB, const QJsonArray& voltB, const QJsonArray& distC, const QJsonArray& voltC);
    void saveCsvFileAndUpdateDb(int  modeName ,QString saveTimeInMysql,QString fileName, QString fullPath);
    void getNewdatafromDB(QString category);
    int getfileNamePattern();
    QMap<QString, QJsonArray> distMap;
    QMap<QString, QJsonArray> voltMap;
    bool Patternstate = false;
    QMap<QString, QJsonArray> PatterndistMap;
    QMap<QString, QJsonArray> PatternvoltMap;
    QSqlDatabase db;
    bool verifyMac();
    QString getPassword();
    QString _dbName;
    QString _user = "root";
    QTimer *autoBackupDateTimer;
    int category_id = 0;
};

#endif // DATABASE_H
