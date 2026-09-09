#ifndef PLCSERVER_H
#define PLCSERVER_H

#include <QDateTime>
#include <QString>
#include "ChatServer.h"
#include <QtCore>
#include <QObject>
#include "Database.h"
#include <QProcess>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include "NetworkMng.h"
#include "DataStorage.h"
#include "SocketClient.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QFile>
#include <QProcess>
#include <cfloat> // For FLT_MAX
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <string>
#include <QtConcurrent> // ต้องมี
#include <QVector>
#include <algorithm>
#include <algorithm>
#include <vector>
#include <utility>
#define SwVersion "V3.0.0 22052026"
// #define SwVersion "V2.0.9 22052026"
#define HwVersion "Raspberry Pi"
#define HwName "OpenPLCSever"

#define RELAY_OPERATE  "OPERATE_TIME"
#define NETWORK_SERVER  "NETWORK"
#define SNMP_SERVER     "SNMP_SERVER"
#define SMTP_SERVER     "SMTP_SERVER"
#define TIME_SERVER     "TIME_SERVER"
#define DELAYS     "DELAYS"
#define EMAIL_PATH     "EMAIL"
#define FTP_PATH  "FTP_SERVER"

#define FILESETTING "/home/pi/.config/plc/settings.ini"

using namespace std::chrono;

class PLCServer : public QObject
{
    Q_OBJECT
public:
    explicit PLCServer(QObject *parent = nullptr);
    ~PLCServer();
    bool open_interlock=false;
    int couunter_loop = 0, couunter_loop2 = 0;
    ChatServer *server;
    SocketClient *client,*plc_client;
    DataStorage *datastorage;
    QTimer *recheckParam= nullptr,*loopTimer= nullptr, *loopNetworkTimer= nullptr, *loopWaitPic= nullptr, *loopLogout= nullptr, *loopLogoutCounter= nullptr, *loopLogoutVNC= nullptr, *loopLogoutCounterVNC= nullptr;
    NetworkMng *networking;
    QWebSocket *OpenPLC_address, *FPGA_address, *snmp_address ,*PLCserver_address, *INPUT_PLC_address, *vnc_address, *MonitorFirst_address, *MonitorSecond_address;
    QList<QWebSocket *> Monitor_address,webapp_address; //
    QTimer *operateTime = nullptr;
    int relayOperateTime=0;

    int reconnect_count=0;
    static void* ThreadFunc( void* pTr );
    typedef void * (*THREADFUNCPTR)(void *);
    pthread_t idThread;

    static void* ThreadFuncNew( void* pTr );
    typedef void * (*THREADFUNCPTRNEW)(void *);
    pthread_t idThreadNew;

    static void* ThreadFunc2( void* pTr );
    typedef void * (*THREADFUNCPTR2)(void *);
    pthread_t idThread2;

    static void* ThreadFunc3( void* pTr );
    typedef void * (*THREADFUNCPTR3)(void *);
    pthread_t idThread3;

    static void* ThreadFunc4( void* pTr );
    typedef void * (*THREADFUNCPTR4)(void *);
    pthread_t idThread4;

    static void* ThreadFunc5( void* pTr );
    typedef void * (*THREADFUNCPTR5)(void *);
    pthread_t idThread5;

    bool changeIPAddress=false;
    int LogoutVNCCount = 0,LogoutCounterVNCCount = 0;
    bool stopThread2 = false; // Signal thread to stop
    bool stopThread3 = false; // Signal thread to stop
    bool stopThread4 = false; // Signal thread to stop
    bool stopThread5 = false; // Signal thread to stop

    QString testNoneSmoothA,testNoneSmoothB,testNoneSmoothC;
    QJsonArray patternNoneSmoothA,patternNoneSmoothB,patternNoneSmoothC;

    bool surge_event_check=false;
    bool adc_event_check=false;

    QString temp_adc_chanel = "";
    QString temp_name = "";
    QString temp_url = "";
    QString temp_date = "";
    QString temp_timefile = "";
    QString temp_timestamp = "";
    QString temp_testMode = "";

    bool patternSelected = false;
    void lftpSendFunction();
    QString remote_link;
    QJsonArray voltBkup,distBkup,voltBkupRemote,distBkupRemote;
    bool SurgeMode = false,SurgeModeRemote = false;
    int pointIntervalSurge;
    float currentDistanceSurge;
    int fullPointRemote,fullPointLocal;
    QString slave = "192.168.10.62";
    QString dateStr = "";
    QString timeStr = "";
    void checkConnected();
    QString FileTimeStamp;
    QString getChrrentDateTime();
    QString timeRec,dateRec,nanosecRec,testModeRec,chanelRec;
    QString LastNanosecRec, LastTestModeRec = "";
    QString PIC_PATH = "/mnt/sdcard/event_records/Pic";
    QString MANUAL_PATH = "/mnt/sdcard/event_records/Manual";
    QString RELAY_PATH = "/mnt/sdcard/event_records/Relay";
    QString SURGE_PATH = "/mnt/sdcard/event_records/Surge";
    QString PATTERN_PATH = "/mnt/sdcard/event_records/Pattern";
    QString PERIODIC_PATH = "/mnt/sdcard/event_records/Periodic";
    QString EVENT_PATH = "/mnt/sdcard/event_records/";
    QString TOWER_NO = "/mnt/sdcard/tower/";
    QString ipaddress_monitor;
    bool change_monitor=false;
//    QString selectPatter="";
    QString ftpUrlFolder,ftpUrlFolder2,ftpUrlFolder3,uploadsFTP;
    QString nanoPhaseA,nanoPhaseB,nanoPhaseC;
    QString nanoPhaseASlave,nanoPhaseBSlave,nanoPhaseCSlave;
    int nanoSecCount = 0;
    bool reset_ip = false;
    void getReadyFolder();
    void checkFolderExist();
    void sendUpdateToFPGA();
    void sendUpdateToMonitor();
    void getPATHReadme();
    QString keepDateTemp;
    void findClosestDistance(const QList<float> &distances, float realDistance);
    int indexclosestValue;
    float closestValue;

    struct Version{
        QString FPGA_version;
        QString Monitor_version;
        bool operator==(const Version& other) const {
            return (/*ipSnmpServer == other.ipSnmpServer &&*/
                    FPGA_version == other.FPGA_version &&
                    Monitor_version == other.Monitor_version );
        }

        // Inequality operator (optional but useful)
        bool operator!=(const Version& other) const {
            return !(*this == other);
        }
    };
    Version *version;

    struct Network{
        // network
        QString dhcpmethod;
        QString ip_address = "127.0.0.1";
        QString subnet;
        QString ip_gateway = "";
        QString pridns;
        QString secdns;
        QString phyName = "eth0";

        //snmp
        QString ip_snmp = "";
        QString location_snmp = "";
        //smtp
//        QString sender_email = "";
//        QString sender_name = "";
//        QString password = "";
//        QString recipient_email = "";
//        QString recipient_name = "";
//        QString server = "";
//        QString port = "";

        //ntp server
        QString ip_timeserver = "";

        bool operator==(const Network& other) const {
            return (/*ipSnmpServer == other.ipSnmpServer &&*/
                    dhcpmethod == other.dhcpmethod &&
                    ip_address == other.ip_address &&
                    subnet == other.subnet &&
                    ip_gateway == other.ip_gateway &&
                    pridns == other.pridns &&
                    secdns == other.secdns &&
                    phyName == other.phyName );
        }

        // Inequality operator (optional but useful)
        bool operator!=(const Network& other) const {
            return !(*this == other);
        }


        void printinfo(){
            qDebug() << "networks dhcpmethod:" << dhcpmethod << " ip_address:" << ip_address
                     << " subnet:" << subnet << " ip_gateway:" << ip_gateway << " pridns:" << pridns
                     << " secdns:" << secdns << " phyName:" << phyName << " ip_timeserver:" << ip_timeserver;
//            qDebug() << " ip_snmp:" << ip_snmp << " sender_email:" << sender_email
//                     << " sender_name:" << sender_name << " password:" << password
//                     << " recipient_email:" << recipient_email << " recipient_name:" << recipient_name
//                     << " server:" << server << " port:" << port << " ip_timeserver:" << ip_timeserver;
        }
    };
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
            qDebug() << "main printinfo FTP_Param FTP_IP:" << FTP_IP
                     << " USERNAME:" << USERNAME << " PASSWORD:" << PASSWORD
                     << " PERIODIC_FILE:" << PERIODIC_FILE << " RELAY_FILE:" << RELAY_FILE
                     << " SURGE_FILE:" << SURGE_FILE << " MANUAL_FILE:" << MANUAL_FILE
                     << " PATTERN_FILE:" << PATTERN_FILE;
        }
    };



    struct EventAlarmHistory{
        QString date = "";
        QString time = "";
        QMap<QString, QString> event_name;
        bool status = true;
        QStringList eventDate,eventTime,eventName,eventStatus;

        void setup(){
            event_name["PLC_DO_ERROR"] = "DEACTIVE";
            event_name["PLC_DI_ERROR"] = "DEACTIVE";
            event_name["MODULE_HI_SPEED_PHASE_A_ERROR"] = "DEACTIVE";
            event_name["MODULE_HI_SPEED_PHASE_B_ERROR"] = "DEACTIVE";
            event_name["MODULE_HI_SPEED_PHASE_C_ERROR"] = "DEACTIVE";
            event_name["INTERNAL_PHASE_A_ERROR"] = "DEACTIVE";
            event_name["INTERNAL_PHASE_B_ERROR"] = "DEACTIVE";
            event_name["INTERNAL_PHASE_C_ERROR"] = "DEACTIVE";
            event_name["GPS_MODULE_FAIL"] = "DEACTIVE";
            event_name["SYSTEM_INITIAL"] = "DEACTIVE";
            event_name["COMMUNICATION_ERROR"] = "DEACTIVE";
            event_name["RELAY_START_EVENT"] = "DEACTIVE";
            event_name["SURGE_START_EVENT"] = "DEACTIVE";
            event_name["PERIODIC_TEST_EVENT"] = "DEACTIVE";
            event_name["MANUAL_TEST_EVENT"] = "DEACTIVE";
            event_name["LFL_FAIL"] = "DEACTIVE";
            event_name["LFL_OPERATE"] = "DEACTIVE";
            for (auto it = event_name.begin(); it != event_name.end(); ++it) {
                qDebug() << it.key() << "->" << it.value();
            }
        }
        void setEvent(QString event, QString date, bool state)
        {
            event = event.trimmed();

            QStringList words = date.split(" ", Qt::SkipEmptyParts);

            if (words.size() < 2) {
                qDebug() << "[setEvent] invalid date format:" << date;
                return;
            }

            QString temp;
            if (state == 1) {
                temp = "ACTIVE";
            } else {
                temp = "DEACTIVE";
            }

            /*
     * สอง event นี้ให้บันทึกทุกครั้ง
     * โดยไม่ต้องเช็คว่าอยู่ใน event_name หรือไม่
     * และไม่ต้องเช็ค duplicate status
     */
            bool forceLog =
                (event == "Send Master Start" ||
                 event == "Receive Master Start");

            if (forceLog) {
                eventDate.append(words[0]);
                eventTime.append(words[1]);
                eventName.append(event);
                eventStatus.append(temp);

                event_name[event] = temp;

                qDebug() << "[setEvent] append forceLog"
                         << "event =" << event
                         << "date =" << words.value(0)
                         << "time =" << words.value(1)
                         << "status =" << temp;

                return;
            }

            /*
     * event ปกติ ใช้ logic เดิม:
     * ต้องมี key ใน event_name และ status ต้องเปลี่ยนเท่านั้น
     */
            for (auto it = event_name.begin(); it != event_name.end(); ++it) {

                if (event == it.key() && temp != it.value()) {

                    eventDate.append(words[0]);
                    eventTime.append(words[1]);
                    eventName.append(event);
                    eventStatus.append(temp);

                    it.value() = temp;

                    qDebug() << "[setEvent] append normal"
                             << "event =" << event
                             << "date =" << words.value(0)
                             << "time =" << words.value(1)
                             << "status =" << temp;

                    return;
                }
            }

            qDebug() << "[setEvent] event not found or duplicate"
                     << "event =" << event
                     << "status =" << temp;
        }
        void clear(){
            eventDate.clear();
            eventTime.clear();
            eventName.clear();
            eventStatus.clear();
        }
    };
    EventAlarmHistory *eventHistory;
    int updateHistory = 0;

    struct EMAIL_Param{
//        QString ipSnmpServer = "192.168.10.191";
        bool PLC_DO_ERROR_MAIL = false;
        bool PLC_DI_ERROR_MAIL = false;
        bool MODULE_HI_SPEED_PHASE_A_ERROR_MAIL = false;
        bool MODULE_HI_SPEED_PHASE_B_ERROR_MAIL = false;
        bool MODULE_HI_SPEED_PHASE_C_ERROR_MAIL = false;
        bool INTERNAL_PHASE_A_ERROR_MAIL = false;
        bool INTERNAL_PHASE_B_ERROR_MAIL = false;
        bool INTERNAL_PHASE_C_ERROR_MAIL = false;
        bool GPS_MODULE_FAIL_MAIL = false;
        bool SYSTEM_INITIAL_MAIL = false;
        bool COMMUNICATION_ERROR_MAIL = false;
        bool RELAY_START_EVENT_MAIL = false;
        bool SURGE_START_EVENT_MAIL = false;
        bool PERIODIC_TEST_EVENT_MAIL = false;
        bool MANUAL_TEST_EVENT_MAIL = false;
        bool LFL_FAIL_MAIL = false;
        bool LFL_OPERATE_MAIL = false;
        int DELAY_EVENT_MAIL = 0;
        int DELAY_ALARM_MAIL = 0;
        // Define the equality operator (operator==)
        bool operator==(const EMAIL_Param& other) const {
            return (/*ipSnmpServer == other.ipSnmpServer &&*/
                    PLC_DO_ERROR_MAIL == other.PLC_DO_ERROR_MAIL &&
                    PLC_DI_ERROR_MAIL == other.PLC_DI_ERROR_MAIL &&
                    MODULE_HI_SPEED_PHASE_A_ERROR_MAIL == other.MODULE_HI_SPEED_PHASE_A_ERROR_MAIL &&
                    MODULE_HI_SPEED_PHASE_B_ERROR_MAIL == other.MODULE_HI_SPEED_PHASE_B_ERROR_MAIL &&
                    MODULE_HI_SPEED_PHASE_C_ERROR_MAIL == other.MODULE_HI_SPEED_PHASE_C_ERROR_MAIL &&
                    INTERNAL_PHASE_A_ERROR_MAIL == other.INTERNAL_PHASE_A_ERROR_MAIL &&
                    INTERNAL_PHASE_B_ERROR_MAIL == other.INTERNAL_PHASE_B_ERROR_MAIL &&
                    INTERNAL_PHASE_C_ERROR_MAIL == other.INTERNAL_PHASE_C_ERROR_MAIL &&
                    GPS_MODULE_FAIL_MAIL == other.GPS_MODULE_FAIL_MAIL &&
                    SYSTEM_INITIAL_MAIL == other.SYSTEM_INITIAL_MAIL &&
                    COMMUNICATION_ERROR_MAIL == other.COMMUNICATION_ERROR_MAIL &&
                    RELAY_START_EVENT_MAIL == other.RELAY_START_EVENT_MAIL &&
                    SURGE_START_EVENT_MAIL == other.SURGE_START_EVENT_MAIL &&
                    PERIODIC_TEST_EVENT_MAIL == other.PERIODIC_TEST_EVENT_MAIL &&
                    MANUAL_TEST_EVENT_MAIL == other.MANUAL_TEST_EVENT_MAIL &&
                    LFL_FAIL_MAIL == other.LFL_FAIL_MAIL &&
                    LFL_OPERATE_MAIL == other.LFL_OPERATE_MAIL &&
                    DELAY_EVENT_MAIL == other.DELAY_EVENT_MAIL &&
                    DELAY_ALARM_MAIL == other.DELAY_ALARM_MAIL);
        }

        // Compare specific delay variables
        bool compareDelayParams(const EMAIL_Param& other) const {
            return (DELAY_EVENT_MAIL == other.DELAY_EVENT_MAIL && DELAY_ALARM_MAIL == other.DELAY_ALARM_MAIL);
        }

        // Compare everything except delay variables
        bool compareExceptDelayParams(const EMAIL_Param& other) const {
            return (
                PLC_DO_ERROR_MAIL == other.PLC_DO_ERROR_MAIL &&
                PLC_DI_ERROR_MAIL == other.PLC_DI_ERROR_MAIL &&
                MODULE_HI_SPEED_PHASE_A_ERROR_MAIL == other.MODULE_HI_SPEED_PHASE_A_ERROR_MAIL &&
                MODULE_HI_SPEED_PHASE_B_ERROR_MAIL == other.MODULE_HI_SPEED_PHASE_B_ERROR_MAIL &&
                MODULE_HI_SPEED_PHASE_C_ERROR_MAIL == other.MODULE_HI_SPEED_PHASE_C_ERROR_MAIL &&
                INTERNAL_PHASE_A_ERROR_MAIL == other.INTERNAL_PHASE_A_ERROR_MAIL &&
                INTERNAL_PHASE_B_ERROR_MAIL == other.INTERNAL_PHASE_B_ERROR_MAIL &&
                INTERNAL_PHASE_C_ERROR_MAIL == other.INTERNAL_PHASE_C_ERROR_MAIL &&
                GPS_MODULE_FAIL_MAIL == other.GPS_MODULE_FAIL_MAIL &&
                SYSTEM_INITIAL_MAIL == other.SYSTEM_INITIAL_MAIL &&
                COMMUNICATION_ERROR_MAIL == other.COMMUNICATION_ERROR_MAIL &&
                RELAY_START_EVENT_MAIL == other.RELAY_START_EVENT_MAIL &&
                SURGE_START_EVENT_MAIL == other.SURGE_START_EVENT_MAIL &&
                PERIODIC_TEST_EVENT_MAIL == other.PERIODIC_TEST_EVENT_MAIL &&
                MANUAL_TEST_EVENT_MAIL == other.MANUAL_TEST_EVENT_MAIL &&
                LFL_FAIL_MAIL == other.LFL_FAIL_MAIL &&
                LFL_OPERATE_MAIL == other.LFL_OPERATE_MAIL
            );
        }

        // Inequality operator (optional but useful)
        bool operator!=(const EMAIL_Param& other) const {
            return !(*this == other);
        }

        void printinfo(){
            qDebug() << "PLC_DO_ERROR_MAIL:" << PLC_DO_ERROR_MAIL << " PLC_DI_ERROR_MAIL:" << PLC_DI_ERROR_MAIL
                     << " MODULE_HI_SPEED_PHASE_A_ERROR_MAIL:" << MODULE_HI_SPEED_PHASE_A_ERROR_MAIL << " MODULE_HI_SPEED_PHASE_B_ERROR_MAIL:" << MODULE_HI_SPEED_PHASE_B_ERROR_MAIL
                     << " MODULE_HI_SPEED_PHASE_C_ERROR_MAIL:" << MODULE_HI_SPEED_PHASE_C_ERROR_MAIL << " INTERNAL_PHASE_A_ERROR_MAIL:" << INTERNAL_PHASE_A_ERROR_MAIL
                     << " INTERNAL_PHASE_B_ERROR_MAIL:" << INTERNAL_PHASE_B_ERROR_MAIL << " INTERNAL_PHASE_C_ERROR_MAIL:" << INTERNAL_PHASE_C_ERROR_MAIL
                     << " GPS_MODULE_FAIL_MAIL:" << GPS_MODULE_FAIL_MAIL << " SYSTEM_INITIAL_MAIL:" << SYSTEM_INITIAL_MAIL
                     << " COMMUNICATION_ERROR_MAIL:" << COMMUNICATION_ERROR_MAIL << " RELAY_START_EVENT_MAIL:" << RELAY_START_EVENT_MAIL
                     << " SURGE_START_EVENT_MAIL:" << SURGE_START_EVENT_MAIL << " PERIODIC_TEST_EVENT_MAIL:" << PERIODIC_TEST_EVENT_MAIL
                     << " MANUAL_TEST_EVENT_MAIL:" << MANUAL_TEST_EVENT_MAIL << " LFL_FAIL_MAIL:" << LFL_FAIL_MAIL
                     << " LFL_OPERATE_MAIL:" << LFL_OPERATE_MAIL << " DELAY_MAIL:" << DELAY_EVENT_MAIL
                     << " DELAY_ALARM_MAIL:" << DELAY_ALARM_MAIL;
        }
    };

    struct Param{
//        QString ipSnmpServer = "192.168.10.191";
        bool PLC_DO_ERROR = false;
        bool PLC_DI_ERROR = false;
        bool MODULE_HI_SPEED_PHASE_A_ERROR = false;
        bool MODULE_HI_SPEED_PHASE_B_ERROR = false;
        bool MODULE_HI_SPEED_PHASE_C_ERROR = false;
        bool INTERNAL_PHASE_A_ERROR = false;
        bool INTERNAL_PHASE_B_ERROR = false;
        bool INTERNAL_PHASE_C_ERROR = false;
        bool GPS_MODULE_FAIL = false;
        bool SYSTEM_INITIAL = false;
        bool COMMUNICATION_ERROR = false;
        bool RELAY_START_EVENT = false;
        bool SURGE_START_EVENT = false;
        bool PERIODIC_TEST_EVENT = false;
        bool MANUAL_TEST_EVENT = false;
        bool LFL_FAIL = false;
        bool LFL_OPERATE = false;
        // Define the equality operator (operator==)
        bool operator==(const Param& other) const {
            return (/*ipSnmpServer == other.ipSnmpServer &&*/
                    PLC_DO_ERROR == other.PLC_DO_ERROR &&
                    PLC_DI_ERROR == other.PLC_DI_ERROR &&
                    MODULE_HI_SPEED_PHASE_A_ERROR == other.MODULE_HI_SPEED_PHASE_A_ERROR &&
                    MODULE_HI_SPEED_PHASE_B_ERROR == other.MODULE_HI_SPEED_PHASE_B_ERROR &&
                    MODULE_HI_SPEED_PHASE_B_ERROR == other.MODULE_HI_SPEED_PHASE_B_ERROR &&
                    INTERNAL_PHASE_A_ERROR == other.INTERNAL_PHASE_A_ERROR &&
                    INTERNAL_PHASE_B_ERROR == other.INTERNAL_PHASE_B_ERROR &&
                    INTERNAL_PHASE_C_ERROR == other.INTERNAL_PHASE_C_ERROR &&
                    GPS_MODULE_FAIL == other.GPS_MODULE_FAIL &&
                    SYSTEM_INITIAL == other.SYSTEM_INITIAL &&
                    COMMUNICATION_ERROR == other.COMMUNICATION_ERROR &&
                    RELAY_START_EVENT == other.RELAY_START_EVENT &&
                    SURGE_START_EVENT == other.SURGE_START_EVENT &&
                    PERIODIC_TEST_EVENT == other.PERIODIC_TEST_EVENT &&
                    MANUAL_TEST_EVENT == other.MANUAL_TEST_EVENT &&
                    LFL_FAIL == other.LFL_FAIL &&
                    LFL_OPERATE == other.LFL_OPERATE );
        }
        // Inequality operator (optional but useful)
        bool operator!=(const Param& other) const {
            return !(*this == other);
        }

        bool fn_fail(){
            return (
            MODULE_HI_SPEED_PHASE_A_ERROR == true ||
            MODULE_HI_SPEED_PHASE_B_ERROR == true ||
            MODULE_HI_SPEED_PHASE_C_ERROR == true ||
            INTERNAL_PHASE_A_ERROR == true ||
            INTERNAL_PHASE_B_ERROR == true ||
            INTERNAL_PHASE_C_ERROR == true ||
            GPS_MODULE_FAIL == true ||
            SYSTEM_INITIAL == true ||
            COMMUNICATION_ERROR == true );
        }

        bool fn_operate(){
            return (
            PLC_DO_ERROR == true ||
            PLC_DI_ERROR == true ||
            RELAY_START_EVENT == true ||
            SURGE_START_EVENT == true ||
            PERIODIC_TEST_EVENT == true ||
            MANUAL_TEST_EVENT == true );
        }

        void printinfo(){
            qDebug() << "PLC_DO_ERROR:" << PLC_DO_ERROR << " PLC_DI_ERROR:" << PLC_DI_ERROR
                     << " MODULE_HI_SPEED_PHASE_A_ERROR:" << MODULE_HI_SPEED_PHASE_A_ERROR << " MODULE_HI_SPEED_PHASE_B_ERROR:" << MODULE_HI_SPEED_PHASE_B_ERROR
                     << " MODULE_HI_SPEED_PHASE_C_ERROR:" << MODULE_HI_SPEED_PHASE_C_ERROR << " INTERNAL_PHASE_A_ERROR:" << INTERNAL_PHASE_A_ERROR
                     << " INTERNAL_PHASE_B_ERROR:" << INTERNAL_PHASE_B_ERROR << " INTERNAL_PHASE_C_ERROR:" << INTERNAL_PHASE_C_ERROR
                     << " GPS_MODULE_FAIL:" << GPS_MODULE_FAIL << " SYSTEM_INITIAL:" << " COMMUNICATION_ERROR" << COMMUNICATION_ERROR
                     << " RELAY_START_EVENT:" << RELAY_START_EVENT << " SURGE_START_EVENT:" << SURGE_START_EVENT
                     << " PERIODIC_TEST_EVENT:" << PERIODIC_TEST_EVENT << " MANUAL_TEST_EVENT:" << MANUAL_TEST_EVENT;
        }
    };
        struct EmailConfig {
            QString senderEmail;
            QString senderName;
            QString password;
            QString recipientEmail;
            QString recipientName;
            QString smtpServer;
            int smtpPort = 0;
            bool operator==(const EmailConfig& other) const {
                return (senderEmail == other.senderEmail &&
                        senderName == other.senderName &&
                        password == other.password &&
                        recipientEmail == other.recipientEmail &&
                        recipientName == other.recipientName &&
                        smtpServer == other.smtpServer &&
                        smtpPort == other.smtpPort);
            }
            bool operator!=(const EmailConfig& other) const {
                return !(*this == other);
            }
    };

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

    struct IO{
        uint8_t INPUT[8] = {};
        uint8_t TR[8] = {};
        uint8_t RELAY[8] = {};
        // Define the equality operator (operator==) with logging
        bool operator==(const IO& other) const {
            bool isEqual = true;

            // Check INPUT array
            for (size_t i = 0; i < 8; ++i) {
                if (INPUT[i] != other.INPUT[i]) {
                    qDebug() << "Mismatch in INPUT at index" << i
                             << ": expected" << INPUT[i]
                             << ", got" << other.INPUT[i];
                    isEqual = false;
                }
            }

            // Check TR array
            for (size_t i = 0; i < 8; ++i) {
                if (TR[i] != other.TR[i]) {
                    qDebug() << "Mismatch in TR at index" << i
                             << ": expected" << TR[i]
                             << ", got" << other.TR[i];
                    isEqual = false;
                }
            }

            // Check RELAY array
            for (size_t i = 0; i < 8; ++i) {
                if (RELAY[i] != other.RELAY[i]) {
                    qDebug() << "Mismatch in RELAY at index" << i
                             << ": expected" << RELAY[i]
                             << ", got" << other.RELAY[i];
                    isEqual = false;
                }
            }

            return isEqual;
        }

        // Inequality operator (optional but useful)
        bool operator!=(const IO& other) const {
            return !(*this == other);
        }
    };
    struct Days{
        bool Monday;
        bool Tuesday;
        bool Wednesday;
        bool Thursday;
        bool Friday;
        bool Saturday;
        bool Sunday;
        QString times;

        bool operator==(const Days& other) const {
            return (Monday == other.Monday &&
                    Tuesday == other.Tuesday &&
                    Wednesday == other.Wednesday &&
                    Thursday == other.Thursday &&
                    Friday == other.Friday &&
                    Saturday == other.Saturday &&
                    Sunday == other.Sunday );
        }
        bool operator!=(const Days& other) const {
            return !(*this == other);
        }
        void printinfo(){
            qDebug() << "Monday:" << Monday << " Tuesday:" << Tuesday
                     << " Wednesday" << Wednesday << " Thursday:" << Thursday
                     << " Friday:" << Friday << " Saturday:" << Saturday << " Sunday:" << Sunday;
        }
        };

    struct GPS{
        bool PPS = true;
        bool GPS_Data = true;
        bool operator==(const GPS& other) const {
            return (PPS == other.PPS &&
                    GPS_Data == other.GPS_Data );
        }
        bool operator!=(const GPS& other) const {
            return !(*this == other);
        }

        bool compare(){
            return (
            PPS == true &&
            GPS_Data == true );
        }

        void print(){
            qDebug() << "PPS:" << PPS
                     << " GPS_Data:" << GPS_Data;
        }
    };

    struct AUX{
        bool fails = false;
        bool operate = false;
        bool operator==(const AUX& other) const {
            return (fails == other.fails &&
                    operate == other.operate );
        }
        bool operator!=(const AUX& other) const {
            return !(*this == other);
        }
        void printinfo(){
            qDebug() << "printinfo AUX fails:" << fails << " operate:" << operate;
        }
    };

    struct INPUT_DELAY {
        uint8_t gpio_0 = 0;
        uint8_t gpio_1 = 1;
        uint8_t gpio_2 = 2;
        uint8_t gpio_3 = 3;
        uint8_t gpio_4 = 4;
        uint8_t gpio_5 = 5;
        uint8_t gpio_6 = 6;
        uint8_t gpio_7 = 7;

        int input_delay0 = 0;
        int input_delay1 = 0;
        int input_delay2 = 0;
        int input_delay3 = 0;
        int input_delay4 = 0;
        int input_delay5 = 0;
        int input_delay6 = 0;
        int input_delay7 = 0;

        int delay0 = 0;
        int delay1 = 0;
        int delay2 = 0;
        int delay3 = 0;
        int delay4 = 0;
        int delay5 = 0;
        int delay6 = 0;
        int delay7 = 0;
        bool operator==(const INPUT_DELAY& other) const {
            return (input_delay0 == other.input_delay0 &&
                    input_delay1 == other.input_delay1 &&
                    input_delay2 == other.input_delay2 &&
                    input_delay3 == other.input_delay3 &&
                    input_delay4 == other.input_delay4 &&
                    input_delay5 == other.input_delay5 &&
                    input_delay6 == other.input_delay6 &&
                    input_delay7 == other.input_delay7 &&
                    delay0 == other.delay0 &&
                    delay1 == other.delay1 &&
                    delay2 == other.delay2 &&
                    delay3 == other.delay3 &&
                    delay4 == other.delay4 &&
                    delay5 == other.delay5 &&
                    delay6 == other.delay6 &&
                    delay7 == other.delay7 );
        }
        bool operator!=(const INPUT_DELAY& other) const {
            return !(*this == other);
        }
    };
    INPUT_DELAY *delays, *temp_delays;
    bool getLFL_Fail,getLFL_Operate;
    Days *day,*lastStateday;
    Param *OpenPLC_param, *Monitor_param, *FPGA_param, *snmp_param ,*PLCserver_param;
    EmailConfig *Email_Config;
    EMAIL_Param *Email_Param,*Email_ParamTemp;
    FTP_Param *FTP_Param_;
    SetupParameterEquipment *SetupEquipment;
    IO *OpenPLC_IO, *Monitor_IO, *FPGA_IO, *snmp_IO ,*PLCserver_IO;
    Network *networks,*networksTemp;
    GPS *gps;
    void sendDelayToClient(QWebSocket *);
    void getSetting();
    void updateNetwork();
    void updateFTPServer();
    void updateEmailParam();
    void updateEmailDelay();
    void sendEmailParamToWeb();
    void sendEmailParamToSNMP();
    void sendFTPParam();
    void updateDelay();
    void updateSNMP();
    void updateSMTP();
    void updateNTP();
    void updateLocation();
    void updateNetwork(quint8 DHCP, QString LocalAddress, QString Netmask, QString Gateway, QString DNS1, QString DNS2,QString phyNetworkName);
    QString readLine(QString fileName);
    QString getUPTime();
    void sendInitial(QWebSocket *);
    bool interlock_recheck = false;
    void updateFirmware();
    QStringList findFile();
    void scanFileUpdate();
    bool foundfileupdate = false;
    int updateStatus = 0;
    bool adcFinish = false;
    bool connectOpenPLC = false;
    int plc_connect = 3;
    int countTostart = 0;
    bool cannot_send = false;
    bool OpenPLC_keepAlive = false, snmp_keepAlive = false, FPGA_keepAlive = false,input_keepAlive = false, plcServer_keepAlive = false;
    int OpenPLC_count = 0, snmp_count = 0, FPGA_count = 0, input_count = 0, plcServer_count = 0;
    QString rawdataSurge;
    QString rawdataArrayA;
    QString rawdataArrayB;
    QString rawdataArrayC;
    QString rawdataPatternArrayA;
    QString rawdataPatternArrayB;
    QString rawdataPatternArrayC;
    void plotGraphA(double,double,double,double,double,double);
    void plotGraphB(double,double,double,double,double,double);
    void plotGraphC(double,double,double,double,double,double);
    void reSamplingNormalizationA(const std::vector<std::pair<float, float>>& result);
    void reSamplingNormalizationB(const std::vector<std::pair<float, float>>& result);
    void reSamplingNormalizationC(const std::vector<std::pair<float, float>>& result);
    void reSamplingNormalizationPatternA(const std::vector<std::pair<float, float>>& result);
    void reSamplingNormalizationPatternB(const std::vector<std::pair<float, float>>& result);
    void reSamplingNormalizationPatternC(const std::vector<std::pair<float, float>>& result);
    void surgeEvent(double,int,int,int,int,int,QString);
    void surgeEventRemote(double,double,double,int,int,int,QString);

    static void* ThreadFuncA(void* pTr);
    static void* ThreadFuncB(void* pTr);
    static void* ThreadFuncC(void* pTr);

    void startThreads(double sagFactorInit, double samplingRateInit, double distanceToStartInit,
                      double distanceToShowInit, double fulldistance, double thresholdInitA,
                      double thresholdInitB, double thresholdInitC);

    void plotPatternA(double,double,double,double,double,double);
    void plotPatternB(double,double,double,double,double,double);
    void plotPatternC(double,double,double,double,double,double);

    void getRawDataADC(QString,QString,QString,QString,QString,QString);
    void getRawDataADCRemote(QString);
    void getPicturePlot(QString,QString,QString);
    void sendSocketThreeDevice();
    void RecalculateWithMargin(QString);
    void RecalculateWithMarginManual(QWebSocket *);
    void RecalculateWithMarginManual(QJsonArray dis, QJsonArray volt, QString phase);
    void sendDIOActive();
    void sendDIOInctive();

    void calcSegmentRange(double totalDistance,
                          int segmentCount,
                          int segmentIndex,
                          double &startpoint,
                          double &endpoint);
    bool isDistanceInSegment(double distance,
                             double startpoint,
                             double endpoint,
                             bool isLastSegment);

    // Client connect to GPS
    QString serverAddress = "";
    int serverPort = 0;
    bool lastState = false;

    // paatern modew
    bool interlockPattern = false;
    bool interlockPressPattern = false;
    int numOfPattern = 0;
    int maxNumOfPattern = 0;
    QList<QList<double>> voltageListA,maxVoltageListA,voltageListB,maxVoltageListB,voltageListC,maxVoltageListC;
    QList<QList<float>> kmListA,maxKmListA,kmListB,maxKmListB,kmListC,maxKmListC;
    std::vector<std::pair<float, float>> resultMaxListA,resultMaxListB,resultMaxListC,resultMaxListSurge,resultMaxListSurgeRemote;
    bool findMaxEachIndex(QString phase,QList<QList<double>> &voltList , QList<QList<float>> &disList,QList<QList<double>> &MaxVoltList , QList<QList<float>> &MaxDisList, std::vector<std::pair<float, float>> &result);
    QTimer *patternTimer;

    // once time
    bool onceTime = false;
    bool gpsLock= false;

    //updateEvent updateEvent
    bool updateEvent = false;
    int updateEventCount = 0;
    QString eventText="";

    // master slave
    QString masterLFL = "";
    QString masterLFLBkup = "";
    QString masterIP = "",slaveIP = "";

    void initMaster();
    void selectUserFromWeb();
    QString FullName, csvUrl, picUrl, picName;

    // operate and fails;
    AUX *tempAux,*Aux;
//    bool aux_operate, aux_fails;
    void updateProgram();
    int language = 0;
    void selectProgram(int);

    int distanceA=0,distanceB=0,distanceC=0;
    QString towerA,towerB,towerC;
    QString fullPathPattern="";
    QString fullPicPATH="";
    QString fileNamePic = "";
    bool fullstate=false;

    int GPS_Count=0;

    bool interlockVNC=false;
    int count_reset = 0;
    static constexpr int kResetThreshold = 60;

    bool standAlone=false;

private:
    qint64 m_lastResetFpgaMs = 0;
    qint64 m_lastEventStorageCheckMs = 0;

signals:
    void getEventAlarmHistory();
    void checkEventStorageAndEmergencyCleanupSignal();
    void setNTPServer(QString);
    void calldeleteOldFilesAndRecords();
    void updateSetupEquipment(SetupParameterEquipment *);
    void updateMasterMode(QString,QString,QString);
    void updatePathNotMount(QString,QString);
    void uploadCSVTower();
    void updateFTPParam(FTP_Param *);
    void sendToSocketPLC(QString);
    void sendToSocket(QString);
    void sendToMonitor(QString);
    void sendToVNC(QString);
    void broadcastMessage();
    void sendMessage(QString, QWebSocket *);
    void broadcastMessage(QString);

    void getEditDatafromMySQLA(QString msg);
    void getDistanceandDetailA(QString msg);
    void getDistanceandDetailB(QString msg);
    void getDistanceandDetailC(QString msg);
    void getTablePhaseA(QString msg);
    void getTablePhaseB(QString msg);
    void getTablePhaseC(QString msg);
    void updateTablePhaseA(QString msg);
    void updateTablePhaseB(QString msg);
    void updateTablePhaseC(QString msg);
    void deletedMySQLA(QString msg);
    void deletedMySQLB(QString msg);
    void deletedMySQLC(QString msg);
    void parameterMarginA(QString msg);
    void parameterMarginB(QString msg);
    void parameterMarginC(QString msg);
    void parameterThreshold(QString msg);
    void getDataThreshold();
    void settingGeneral();
    void preiodicSetting();
    void updateTimer(QString);
    void updateWeekly(QString);
    void updateUser(QString);
    void updateRelay(QString);
    void rawdataPlot(QString);
    void cursorDistance(QString);
    void moveCursor(QString);
    void changeDistanceRange(QString);
    void taggingpoint(QString);
    void clearDataGraph(QString);
    void clearDisplay(QString);
    void settingdisplay(QString);
    void patterPlotGraph(QString);

    ////////////////////pattern datastorage//////////////////////////
   void getdatapatternDataDb();
   void sortnamePattern(bool Sort, const QString &categoryName);
   void sortdatePattern(bool Sort, const QString &categoryDate);
   void searchByName(const QString &name, const QString &categoryName);
   void searchByDate(const QString &date, const QString &categoryDate);
   void ButtonPattern(QString);

   //--------------------Recipientgmail--------------------------//
   void UpdateRecipientgmail(QString gmail ,int id);
   void RemoveRecipientgmail(QString gmail);
   void NewRecipientgmail(QString gmail);

   //---------------------userNameandPassword--------------------------///
   /// \brief updateMargin
   void UpdateuserNameandPassword(QString UserName,QString Password,int UserLevel,int id);
   void RemoveuserNameandPassword(QString UserName,QString Password,int UserLevel);
   void NewuserNameandPassword(QString UserName,QString Password,int UserLevel);

   void UpdateMarginSettingParameter(QString);
   void updataListOfMarginA(QString);
   void updataListOfMarginB(QString);
   void updataListOfMarginC(QString);
   void getMarginUpdate();
   void updateDisplayInfoSetting(QString);
   void sendMarginUpdate(QString);

   void SumNormalizationandUpdateDb(QString modeName,QString phase,QString FileTimeStamp,QJsonArray dist, QJsonArray volt);

   void deleteCsvFileAndFolder(QString fileName,QString category, QString date);
   void getandSenddataFromCSV(QString fileName,QString category, QString dat);
   void getScreenPicture(QString link,QString fileName);
   void uploadFile(QString filePath,QString ftpUrl,QString username,QString password);

   void getuserlogin(QString username , QString password, QWebSocket*);
   void updateTableDataTagging(int No , double Distance , QString  Detail);
   void deleteTableDataTagging(int No);
   void NewPatternFile(QString modeName ,QString Name);
   void SavePatternFile(QString modeName, QString Name ,QString event_datetime);

   //signal database
   void createSelectPatternSignal();
   void createRangeLFLSignal();
   void RangeLFLSignal();
   void SelectPatternSignal();
   void selectDataCSVTowerSignal();
   void selectMarginSettingParameterSignal();
   void selectMasterModeSignal();
   void deleteOldFilesAndRecordsSignal();
   void getdataMysqlSignal();
   void getSmtpParameterSignal();
   void getParameterEquipmentSignal();
   void getLFLSignal();
   void GetSettingDisplaySignal();
   void getSettingInfoSignal();
   void getThresholdSignal();
   void getSettingNetworkSignal();
   void updateMarginSignal();
   void gettowerAndDistanceSignal();
   void getMyTaggingPhaseASignal();
   void getMyTaggingPhaseBSignal();
   void getMyTaggingPhaseCSignal();
   void getdatapatternDataDbSignal();
   void clearFpgaUpdate();
   void clearMonitorUpdate();
   void link_sdcard_web();

private slots:
   void operateTimeSlot();
   void assignDisplayGerneral(double,QString,QString,QString);
   void clearFpgaUpdateBackground();
   void clearMonitorUpdateBackground();
   void link_sdcard_webSlot();
   void LogoutVNC();
   void Logout();
   void LogoutCounterVNC();
   void LogoutCounter();
   void sendMessages(QString, QWebSocket*);
   void uploadFTP(QString);
   void sendToFPGA(QString);
   void loopFtpTimerFunction();
   void getSettingNetworks(QString ,QString, QString, QString, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool);
   void uploadToSNMP(QString, QString);
   void updateTimePeriodic(QString);
   void updateDatePeriodic(bool,bool,bool,bool,bool,bool,bool);
//   void ledDelayFunction();
//   void updateEventFunction();
   void patternTimerFunction();
   void assignThreshold(double,double,double);
   void assignGetSettingDisplay(double,double,double,double,double);
   void manageDataClient(QString);
  void manageDataClientPLC(QString);
   void SocketClientError();
    void loopGetInfo();
    void manageData(QString, QWebSocket*);
    void handleManageObject(QWebSocket *wClient, QString msgs);
    void recheck();
    void updateSnmpipServer(QString ip);
    void updateEmailSenderConfig(QString senderEmail,QString senderName,QString  Password,QString smtpServer,int smtpPort);
    void updateSnmpSelectionState();
    void saveipServer();
    void saveEmail_Config();
    void newdataMysql(QString ip,bool plcDO,bool plcDI,bool HiSpeedPhaseA,bool HiSpeedPhaseB,bool HiSpeedPhaseC, bool modbusPhaseA, bool modbusPhaseB, bool modbusPhaseC,
                      bool gpsModule, bool systeminit, bool communication, bool relayStart, bool surgeStart, bool periodic, bool manualTest, bool lflFail, bool lflOperate);

     void newdataSmtpParameter(QString senderEmail,QString NamesenderEmail,QString Password,QString recipientEmail,QString recipientName,QString SmtpServer,int SmtpPort);
     void newdataParameterEquipment( QString SubstationName,int Voltage,QString TransmissionLineName,QString Distance,QString IPaddress,QString Brand,QString Model,QString SerialNo,
                                    QString ContractNumber,QString Date,QString LFLSerialNo);
     void disconnected(QWebSocket *);
     void updateNewRecipientgmail(QString);
//     void SumNormalizationa();
     void getCsvFile(QString fileName, QString category, QString date);
     void getScreenPictureandSave(QString link,QString fileName);

     void uploadFileWithtoftpServer(QString filePath,QString ftpUrl,QString username,QString password);
     bool uplouploadFileWithtoftpServer(QString mode,
                                        QString eventCsvPath,
                                        QString picturePath,
                                        QString pictureName,
                                        QString patternPath = QString(),
                                        QString patternName = QString());
public Q_SLOTS:
     void getCsvMarginFile(QString fileName, QString category, QString date);
     void loopWaitPicSlot();
     void updatePATHEmail(QString,QString);
     void selectMasterMode(QString,QString,QString);
     void calculate(QString);
     void updateMode(QString);
     void loopNetwork();
     void sendToWeb();
     void assignPATHCSV(QString,QString);
     void startFtpTimer(QString, QString, QString);
     void calFLF(QJsonArray,QJsonArray,QString);
     void patternSelectDelete(QString, QString, QString);

 public Q_SLOTS:
     void clearAppCacheAndSwap(bool dropSystemCache = true, bool clearSwap = false);

private:
    // ---------------------------------------------------------------------
    // Event Process Audit / Web checklist telemetry
    // ---------------------------------------------------------------------
    // Every event uses one eventId from start until completion.  Each process
    // update is persisted as JSONL under EVENT_PATH/Audit and streamed to
    // webapp_address as objectName=EVENT_PROCESS_AUDIT.  Web UI is external;
    // PLCServer only publishes JSON telemetry through its existing WebSocket.
    void startEventAudit(const QString &mode,
                         const QString &eventDate,
                         const QString &eventTime,
                         const QString &eventName,
                         const QString &identityHint = QString());
    void ensureEventAuditContext(const QString &mode,
                                 const QString &eventDate,
                                 const QString &eventTime,
                                 const QString &eventName,
                                 const QString &identityHint = QString());
    void auditEventStep(const QString &processCode,
                        const QString &processName,
                        const QString &status,
                        const QString &description,
                        const QString &source = QString(),
                        const QString &destination = QString(),
                        const QJsonObject &details = QJsonObject());
    void finishEventAudit(bool pass, const QString &description);
    void auditEventDataPhase(const QString &phase,
                             const QString &fileTimeStamp,
                             const QJsonArray &distanceData,
                             const QJsonArray &voltageData);
    void sendEventAuditToWeb(const QJsonObject &record);
    void appendEventAuditLocal(const QJsonObject &record);
    QJsonObject eventAuditMachineSnapshot() const;
    QJsonObject eventAuditRuntimeSnapshot() const;
    QJsonObject buildEventAuditSnapshot() const;
    void sendEventAuditSnapshot(QWebSocket *client);

    bool runFtpCommandNow(QString command, int timeoutMs = 45000);
    bool isVerifiedLocalFile(const QString &path, qint64 *size = nullptr) const;
    QString resolveExistingPicturePath(const QString &storedPath,
                                       const QString &picName,
                                       const QString &eventDate,
                                       const QString &eventTime) const;
    QString resolveExistingEventCsvPath(const QString &storedPath,
                                        const QString &csvName,
                                        const QString &mode,
                                        const QString &eventDate,
                                        const QString &eventTime) const;
    // Build the legacy SYNC path fields only from the exact event transaction
    // that already passed mandatory local verification + FTP download-back
    // verification.  This method performs another physical verification at
    // actual SYNC publish time so stale/missing files can never be emitted.
    QString canonicalRemoteEventCsvName(const QString &localCsvName,
                                        const QString &eventDate,
                                        const QString &eventTime) const;

    bool buildVerifiedSyncPaths(QString *syncPicPayloadOut,
                                QString *patternPath,
                                QString *failureReason = nullptr) const;
    void padPatternUntilDistance(QVector<double> &distanceArray,
                              QVector<double> &distanceArrayBkup,
                              QJsonArray &distPat,
                              QVector<double> &disPattern,
                              QVector<double> &voltageArray,
                              QVector<double> &voltageArrayBkup,
                              QJsonArray &voltPat,
                              QVector<double> &volPattern,
                              double distanceToShow,
                              double step);
     bool sendDIO = false;
    QString m_lastUrl;
    QString TransmissionLine;
    float FullDistance;
    QList<float> Distance;
    QStringList TowerNo;
    QString fullpathCSV,fullnameCSV;
    QString lastGetCurrentTime = "";
    Database *myDatabase;
    ChatServer *SocketServer;
    QNetworkAccessManager *manager;
    double sagFactor = 0.983;       // SAG factor
    double samplingRate = 60.000; // Sampling rate (meters per sample)
    double distanceToStart; // ระยะตั้งต้น (เมตร)
    double distanceToShow;   // ระยะปลายทาง (เมตร)
    double fulldistance;
    double thresholdA;
    double thresholdB;
    double thresholdC;
    double distancePointBetweenPoint = 60.0f;
    bool isVersion = false;
    double offset;
    int point;
    double PositionFromLocal;
    double PositionFromRemote;
    int local_nanosec;
    int remote_nanosec;
    QString phaseSurge;
    double valuetheshold; //เป็นตัวแปรที่เปลี่ยนแปลงได้
    int samplingrate; //เป็นตัวแปรที่เปลี่ยนแปลงได้ 225
    double SAG; //0.983
    double speedOfligth = 3e8; //m/s
    double timepoint = 200e-9; //s
    double distance = (speedOfligth * timepoint)/100;
    int initiation;//km เป็นตัวแปรที่เปลี่ยนแปลงได้
    double destination;//=8500/distance;//km เป็นตัวแปรที่เปลี่ยนแปลงได้
    int totalpoint = destination - initiation;     //1จุดห่างกัน60เมตร
    int sagfactor;
    int resamplingpoint; //= samplingrate % sagfactor
    QString modeName;
    QJsonArray distA, voltA, distB, voltB, distC, voltC;
    QJsonArray distAPat, voltAPat, distBPat, voltBPat, distCPat, voltCPat;
    QVector<double> disAPattern,disBPattern,disCPattern;
    QVector<double> volAPattern,volBPattern,volCPattern;
    QVector<double> distanceArrayAPattern,distanceArrayBPattern,distanceArrayCPattern;
    QVector<double> distanceArrayAPatternbkup,distanceArrayBPatternbkup,distanceArrayCPatternbkup;
    QVector<double> voltageArrayAPattern,voltageArrayBPattern,voltageArrayCPattern;
    QVector<double> voltageArrayAPatternbkup,voltageArrayBPatternbkup,voltageArrayCPatternbkup;
    QString ftpserverpart,ftpserverpattern = "";
    double timeOfDistance;
    double realDistanceA;
    double realDistanceB;
    double realDistanceC;
    int thelistNumOfMarginA;
    int thelistNumOfMarginB;
    int thelistNumOfMarginC;
    QString DateKept,TimeKept;

    // Authoritative EVENT base timestamp captured from objectName=eventRecord.
    // This is the owner of the FTP/SYNC base folder (yyyy-MM-dd/HH-mm-ss).
    // Picture/ADC timestamps may differ by a few seconds and are correlation
    // data only; they must never move the remote event base folder.
    QString eventBaseDateSnapshot;
    QString eventBaseTimeSnapshot;
    QString eventBaseModeSnapshot;

    // Snapshot used only by the current picture transaction.  For a normal
    // event these values are copied from eventBase* above.  The Picture
    // filename still keeps its real capture timestamp.
    QString pictureEventDateSnapshot;
    QString pictureEventTimeSnapshot;
    QString pictureEventModeSnapshot;

    // Final verified event transaction snapshots. These are populated only after
    // mandatory Picture + Event CSV pass the local gates and the FTP bundle is
    // verified. loopWaitPicSlot() must consume only these values; it must never
    // reconstruct a path from mutable DateKept/TimeKept/fileNamePic/fullPicPATH.
    QString verifiedEventCsvLocalPathSnapshot;
    QString verifiedPictureLocalPathSnapshot;
    // Optional Pattern source file on the PLC filesystem.  Unlike Event CSV
    // and Picture, the SYNC compatibility payload intentionally publishes this
    // verified LOCAL source path rather than the FTP copy.
    QString verifiedPatternLocalPathSnapshot;
    QString verifiedFtpEventCsvRemotePath;
    QString verifiedFtpPictureRemotePath;
    QString verifiedFtpPatternRemotePath; // optional; empty when not uploaded
    bool verifiedEventBundleReady = false;

    // Current event audit state.  The latest state per processCode is retained
    // for web checklist/table snapshots.  Recent completed event summaries are
    // kept in memory (max 20) while the detailed stream is persisted as JSONL.
    bool eventAuditActive = false;
    QString eventAuditId;
    QString eventAuditMode;
    QString eventAuditName;
    QString eventAuditDate;
    QString eventAuditTime;
    QString eventAuditLogPath;
    qint64 eventAuditStartedMs = 0;
    int eventAuditSequence = 0;
    int eventAuditPassCount = 0;
    int eventAuditFailCount = 0;
    int eventAuditRetryCount = 0;
    int eventAuditBlockCount = 0;
    int eventAuditInfoCount = 0;
    int eventAuditSkipCount = 0;
    int eventAuditStartCount = 0;
    QMap<QString, QJsonObject> eventAuditLatestSteps;
    QJsonArray eventAuditHistory;

    int lenghtFLF;
    float currentDistanceA,currentDistanceB,currentDistanceC;

private:
    enum EventPictureState {
        EventPictureIdle = 0,
        EventPictureWaiting,
        EventPictureProcessing,
        EventPictureVerified,
        EventPictureClosed
    };

    QTimer *screenPictureDelayTimer = nullptr;

    // One Event may own only one canonical Picture.  eventRecord opens the
    // ownership window; the first accepted ScreenPicture reserves it.  A later
    // ScreenPicture (same or different filename) is ignored after the Picture
    // has been verified/published.  A failed transaction releases the
    // reservation back to Waiting so a genuine recovery request can retry.
    bool eventPictureActive = false;
    EventPictureState eventPictureState = EventPictureIdle;
    QString activePictureEventId;
    QString acceptedPictureName;

    // sendMail is idempotent per live event.  FTP recovery may re-enter the
    // Picture transaction, but the same event must never generate duplicate
    // mail notifications once one payload has been published to a connected
    // downstream consumer.
    bool eventMailPublished = false;
    QString eventMailPublishedEventId;

    QString pendingScreenLink = "";
    QString pendingScreenFileName = "";
    QString pendingScreenMode = "";
    QString pendingScreenSenderAddress = "";
    quint16 pendingScreenSenderPort = 0;

    QString eventPictureStateName() const;
    void resetEventPictureOwnership(const QString &eventId,
                                    const QString &mode,
                                    const QString &eventDate,
                                    const QString &eventTime);
    void releaseEventPictureReservationForRetry(const QString &pictureName,
                                                const QString &reason);
    void closeEventPictureOwnership(const QString &reason);

private slots:
    void processDelayedScreenPicture();

};
#endif // PLCSERVER_H
