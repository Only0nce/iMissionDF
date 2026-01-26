#ifndef MAINWINDOWS_H
#define MAINWINDOWS_H

#include <QObject>
#include "ChatServer.h"
#include "NetworkController.h"
#include "QTimer"
#include "ReceiverRecorderConfigManager.h"
#include "SocketClient.h"
#include "newGPIOClass.h"
#include <SigmaStudioFW.h>
#include <PCM3168A.h>
#include "websocketclient.h"
#include "OpenWebRxConfig.h"
#include "FileUpdateWatcher.h"
#include "HMC253Controller.h"
#include "Databases.h"
#include <QUuid>
#include "rfdc_nco_client.h"
#include "alsarecconfigmanager.h"
#include <QThread>
#include "SetFreqWorker.h"

#pragma once

#define GPA0   0
#define GPA1   1
#define GPA2   2
#define GPA3   3
#define GPA4   4
#define GPA5   5
#define GPA6   6
#define GPA7   7
#define GPB0   8
#define GPB1   9
#define GPB2  10
#define GPB3  11
#define GPB4  12
#define GPB5  13
#define GPB6  14
#define GPB7  15

#define GPIO00	131     //"479"		//GPIO33_PZ.01		tegra234-gpio = (23*8)+1 = 185
#define GPIO01	105     //"453"		//GPIO33_PQ.05		tegra234-gpio = (15*8)+5 = 125
#define GPIO02	98     //"446"		//GPIO33_PP.06		tegra234-gpio = (14*8)+6 = 118
#define GPIO03	12    //"328"		//GPIO33_PCC.00		tegra234-gpio-aon = (02*8)+0 = 16
#define GPIO04	13    //"329"		//GPIO33_PCC.01		tegra234-gpio-aon = (02*8)+1 = 17
#define GPIO05 	14    //"330"		//GPIO33_PCC.02		tegra234-gpio-aon = (02*8)+2 = 18
#define GPIO06	15    //"331"		//GPIO33_PCC.03		tegra234-gpio-aon = (02*8)+3 = 19
#define GPIO07  41     //"389"		//GPIO33_PG.06		tegra234-gpio = (06*8)+6 = 54
#define GPIO08  102     //"450"		//GPIO33_PQ.02		tegra234-gpio = (15*8)+2 = 122 // NV_THERM_FAN_TACH0
#define GPIO09	144    //"492"		//GPIO33_PAC.06		tegra234-gpio = (24*8)+6 = 198
#define GPIO10	25   //"341"		//GPIO33_PEE.02		tegra234-gpio-aon = (04*8)+2 = 34 //มีปัญหา
#define GPIO11	106     //"454"		//GPIO33_PQ.06		tegra234-gpio = (15*8)+6 = 126
#define GPIO12	85     //"433"		//GPIO33_PN.01		tegra234-gpio = (13*8)+1 = 105
#define GPIO13  43     //"391"		//GPIO33_PH.00		tegra234-gpio = (07*8)+0 = 56
#define GPIO14	117     //"465"		//GPIO33_PX.03		tegra234-gpio = (21*8)+3 = 171

#define GPIO_CODEC_RESET    "gpiochip2",GPB5
#define GPIO_DSP_SBOOT      "gpiochip2",GPB6
#define GPIO_DSP_RESET      "gpiochip2",GPB7
#define GPIO_LED3           "gpiochip2",GPA4
#define GPIO_LED4           "gpiochip2",GPA3
#define GPIO_HP_OFF         "gpiochip2",GPA0
#define GPIO_AMP1_SHD       "gpiochip2",GPA1
#define GPIO_AMP1_MUTE      "gpiochip2",GPA2
#define GPIO_BACKLIGHT      "gpiochip4",GPA0

#define GPIO_LNA1_EN      "gpiochip3",GPB5
#define GPIO_LNA2_EN      "gpiochip3",GPB3

#define ROTARY_LED "gpiochip0",GPIO13
#define RST_AMP "gpiochip1",14
#define SHD_AMP "gpiochip1",15
#define HS_MUTE "gpiochip0",102

#define LED_ON   true
#define LED_OFF  false
#define RESET_ACTIVE     false
#define RESET_INACTIVE    true

#define QSPIFLASH       true
#define JETSONSPI       false


class Mainwindows : public QObject
{
    Q_OBJECT

public:

    explicit Mainwindows(QObject *parent = nullptr);
    WebSocketClient wsClient;

    Q_INVOKABLE int center_freq() const { return wsClient.rxconfig.center_freq; }
    Q_INVOKABLE int samp_rate() const { return wsClient.rxconfig.samp_rate; }
    Q_INVOKABLE QString start_mod() const { return wsClient.rxconfig.start_mod; }

    Q_INVOKABLE QVariantList getWaterfallColorMap() const {
        QVariantList list;
        for (int color : wsClient.rxconfig.waterfall_colors)
            list.append(color);
        return list;
    }

    HMC253Controller *hmc = new HMC253Controller;
    unsigned char VolumeOutCH1 = 150;
    unsigned char VolumeOutCH2 = 150;
    unsigned char VolumeOutCH3 = 150;
    unsigned char VolumeOutCH4 = 100;

    unsigned char DSPVolumeOutCH1 = 16;
    unsigned char DSPVolumeOutCH2 = 16;
    unsigned char DSPVolumeOutCH3 = 16;
    unsigned char DSPVolumeOutCH4 = 16;

    unsigned char CODECVolumeOutCH1 = 30;
    unsigned char CODECVolumeOutCH2 = 30;
    unsigned char CODECVolumeOutCH3 = 30;
    unsigned char CODECVolumeOutCH4 = 30;

    unsigned char scanSqlLevel = 100;

    Q_INVOKABLE unsigned char getSpeakerVolume1() const { return VolumeOutCH1; }
    Q_INVOKABLE unsigned char getSpeakerVolume2() const { return VolumeOutCH2; }
    Q_INVOKABLE unsigned char getHeadphoneVolume() const { return VolumeOutCH3; }

    Q_INVOKABLE unsigned char getSqlLevel() const { return scanSqlLevel; }

    double beforeAudioVolume=0.0;
    Q_INVOKABLE void setHeadphoneVolume(const unsigned char volume);
    Q_INVOKABLE void setSpeakerVolume(const unsigned char volume);
    // Q_INVOKABLE void setSpeakerVolumeMute(bool active);
    Q_INVOKABLE void setSqlLevel(const unsigned char value);
    Q_INVOKABLE void updateCurrentOffsetFreq(const int value, const double centerFreq);

    Q_INVOKABLE void refreshProfiles(); // call from QML if needed
    Q_INVOKABLE static QString generateGUID()  {
        return QUuid::createUuid().toString();
    }

    Q_INVOKABLE void shutdownRequested();
    Q_INVOKABLE void rebootRequested();
    Q_INVOKABLE void offScreenRequested();
    Q_INVOKABLE void backlightRequested();
    Q_INVOKABLE void onScreenRequested();

    Q_INVOKABLE void sCanfreq();
    Q_INVOKABLE void sendmessageToWeb(const QString &jsonMessage);

    Q_INVOKABLE void deleteCardWebSlot(QString);
    Q_INVOKABLE void addCardWebSlot(QString);
    Q_INVOKABLE void editCardWebSlot(QString,QString);

    Q_INVOKABLE void deleteScanCardSlot(QString);
    Q_INVOKABLE void deleteScanCardAllSlot();

    Q_INVOKABLE void setNetworkFormDisplay(const QString &ipWithCidr);
    Q_INVOKABLE void setNetworkFormDisplay(const int index,
                                           const QString &mode,
                                           const QString &ipWithCidr,
                                           const QString &gateway,
                                           const QString &dnsList);

    OpenWebRxConfig openWebRxConfig;
    int gpioKeyProfile = 0;

    QString toString(RFPort port) {
        switch (port) {
        case RFPort::RF1: return "RF1";
        case RFPort::RF2: return "RF2";
        case RFPort::RF3: return "RF3";
        case RFPort::RF4: return "RF4";
        case RFPort::RF5: return "RF5";
        case RFPort::RF6: return "RF6";
        case RFPort::RF7: return "RF7";
        case RFPort::RF8: return "RF8";
        default: return "Unknown";
        }
    }

    RFPort selectRFByFreqAndBW(double centerMHz, double bwMHz) {
        double lowFreq = centerMHz - (bwMHz / 2.0);
        double highFreq = centerMHz + (bwMHz / 2.0);

        // ค่าช่วงความถี่ของแต่ละ RF Port
        struct RFRange {
            RFPort port;
            double minFreq;
            double maxFreq;
        };

        const std::array<RFRange, 7> rfRanges = {{
            {RFPort::RF1, 1100, 1575},
            {RFPort::RF2, 672, 1200},
            {RFPort::RF3, 440, 722},
            {RFPort::RF4, 290, 470},
            {RFPort::RF5, 160, 340},
            {RFPort::RF6, 65, 264},
            {RFPort::RF7, 0, 105}
        }};

        // หาพอร์ตแรกที่ครอบคลุมทั้งช่วง [lowFreq, highFreq]
        for (const auto& range : rfRanges) {
            if (lowFreq >= range.minFreq && highFreq <= range.maxFreq) {
                return range.port;
            }
        }

        return RFPort::RF8;  // ไม่สามารถครอบคลุมได้
    }
    Database *myDatabase;
    struct ScanRF{
        std::vector<double> freq;
        QStringList bw;
        QStringList mod;
    };
    struct ScanCard
    {
        int         id;
        double      freq;
        QString     unit;
        QString     bw;
        QString     mode;
        int         low_cut;
        int         high_cut;
        QString     path;
        QDateTime   created_at;
        QDateTime   time;
    };
    QJsonArray cardsProfilesArray;
    QVector<ScanCard> cards;
    ScanRF *scanrf;
    QJsonArray removeCardById(const QJsonArray &array, int targetId);
    QString toThaiTimeString(const QString &isoUtc);
    QString toThaiTimeStringWithoutT(const QString &isoUtc);
    Q_INVOKABLE void deleteScanGroupByKey(const QString &groupKeyThai);
    QString timeLocation = "";
    void setLocation(QString location);

    // ตั้งค่า host / port (เรียกเมื่อไรก็ได้)
    void setHostPort(const QString& host, quint16 port);
    // ส่งคำสั่ง SETFREQ <Hz>
    bool setFreq(quint64 freqHz, int timeoutMs = 1000);

    void setHostPortNc(const QString& host, quint16 port);
    void setHostPortNc();

signals:
    ////DISPLAY
    void updateNetworkToDisplay(QString);



    ///
    ///
    void scanCardUpdateDelete(int id);
    void profilesFromDb(const QVariantList &profiles);
    void insertScanCard(double freq, const QString &unit, const QString &bw, const QString &mode, int lowCut, int highCut, const QString &path, QString time);
    void deleteScanCardById(int id);
    void deleteScanCardAll();
    void deleteScanCardGroup(const QString &groupDateTime);
    void updateListProfiles();
    void deleteAllPresets();
    void deleteSpecificProfile(QString id);
    void deleteScanProfile(QString id);
    void editSpecificProfile(QString id);
    void selectSpecificProfile(const QJsonObject &config);
    void addNewProfile(const QJsonObject &config);
    void updateCardProfile();
    void cppCommand(const QVariant& jsonMsg);
    void spectrumUpdated(QVariantList spectrumData);
    void onTemperatureChanged(double value);
    void onRecStatusChanged(bool recStatus);
    void waterfallUpdated(QVariantList spectrumData);
    void smeterValueUpdated(double smeterValue);
    void waterfallColorUpdate(QVariantList waterfallColors);
    void findBandsWithProfile(QVariant mode);
    void waterfallLevelsChanged(int min, int max);
    void updateCenterFreq();
    void updateProfiles(QJsonArray value);
    void updateGPIOKeyProfiles(int value);
    void updateRotaryProfiles(int dir);
    void onOpenwebrxConnected();
    void onSendSquelchStatus(int softPhoneID, bool pttOn, bool sqlOn, bool callState, double freq);
    void SquelchStatusChange(bool,QString,int,double);

public slots:
    void requestSetFreqAsync(quint64 freqHz, int timeoutMs);
    void initValueJson(const QJsonArray &arr);
    void profileWeb(QString);
    void profiles();
    void onRecorderConfigSaved();
    void cppSubmitTextFiled(const QString&  qmlCommand);
    void sCan(const QString&  qmlCommand);
    void sendmessage(const QString &jsonMessage);
    void onCenterFreqChanged()
    {
        // rfdc->connectToServer();
        // rfdc->setFrequency(center_freq());
        requestSetFreqAsync(center_freq(),200);
        qDebug() << "rfdc->setFrequency(center_freq());";
        double freq = center_freq()/1e6;     // MHz
        double bw = samp_rate()/1e6;        // MHz
        RFPort port = selectRFByFreqAndBW(freq, bw);
        // hmc->selectRF(port);
        hmc->selectRFPair(port);
        emit updateCenterFreq();
        currentCenterFreq = center_freq();
        qDebug() << "Selected RF:" << toString(port) << "current Center Frequency:" << currentCenterFreq;
        // hmc->selectRF(RFPort::RF6);
    }

private:
    QThread m_setFreqThread;
    SetFreqWorker *m_setFreqWorker = nullptr;

    QString nc_host = "127.0.0.1";
    quint16 nc_port = 6000;
    int recRunningCount = -1;
    bool recEnable = true;
    bool currentSQLValue = false;
    double currentCenterFreq = center_freq();//100e6;
    int currentOffsetFreq = 0;
    double currentFreq = 100e6;
    SocketClient *iPatchServerSocket = nullptr;
    QTimer *socketClientReconnectTimer = nullptr;
    ADAU1467* SigmaFirmWareDownLoad = nullptr;
    newGPIOClass *codecReset = new newGPIOClass(GPIO_CODEC_RESET);
    newGPIOClass *dspReset = new newGPIOClass(GPIO_DSP_RESET);
    newGPIOClass *dspBootSelect = new newGPIOClass(GPIO_DSP_SBOOT);
    newGPIOClass *led3 = new newGPIOClass(GPIO_LED3);
    newGPIOClass *led4 = new newGPIOClass(GPIO_LED4);
    newGPIOClass *headphoneGpioOn = new newGPIOClass(GPIO_HP_OFF);
    newGPIOClass *ampGpioStandby = new newGPIOClass(GPIO_AMP1_SHD);
    newGPIOClass *ampGpioMute = new newGPIOClass(GPIO_AMP1_MUTE);
    newGPIOClass *backlight = new newGPIOClass(GPIO_BACKLIGHT);
    newGPIOClass *lna_1_enable = new newGPIOClass(GPIO_LNA1_EN);
    newGPIOClass *lna_2_enable = new newGPIOClass(GPIO_LNA2_EN);
    newGPIOClass *rotary_led = new newGPIOClass(ROTARY_LED);
    newGPIOClass *rst_amp = new newGPIOClass(RST_AMP);
    newGPIOClass *shd_amp = new newGPIOClass(SHD_AMP);
    newGPIOClass *hs_mute = new newGPIOClass(HS_MUTE);
    ChatServer  *wsServer = new ChatServer(8049);
    ChatServer  *webServer = new ChatServer(3310);
    RfdcNcoClient *rfdc = new RfdcNcoClient();
    ReceiverRecorderConfigManager *recConfig = new ReceiverRecorderConfigManager();
    NetworkController *netWorkController = new NetworkController;

    void setRfSwitchBand(RFPort port)
    {
        hmc->selectRF(port);
    }

    QTimer *squelchOffTimer = nullptr;
    QTimer *startScanCard = nullptr;
    bool isSquelchOffPending = false;


    // newGPIOClass *A_IN = new newGPIOClass(GPIO_A_SW_IN);
    // newGPIOClass *B_IN = new newGPIOClass(GPIO_B_SW_IN);
    // newGPIOClass *C_IN = new newGPIOClass(GPIO_C_SW_IN);

    // newGPIOClass *A_OUT = new newGPIOClass(GPIO_A_SW_OUT);
    // newGPIOClass *B_OUT = new newGPIOClass(GPIO_B_SW_OUT);
    // newGPIOClass *C_OUT = new newGPIOClass(GPIO_C_SW_OUT);

    FileUpdateWatcher *fileUpdateWatcher = new FileUpdateWatcher("/var/www/html/uploads", "update.tar");

    PCM3168A * CODEC_PCM3168A = nullptr;

    unsigned char VolumeInCH1 = 10;
    unsigned char VolumeInCH2 = 10;
    unsigned char VolumeInCH3 = 10;
    unsigned char VolumeInCH4 = 10;

    double VolumeOutDSPCH1 = 1; //0.0001 - 1  **** (-80)dB - 0dB 20logx
    double VolumeOutDSPCH2 = 1;
    double VolumeOutDSPCH3 = 1;
    double VolumeOutDSPCH4 = 1;

    double VolumeRecInDSPCH1 = 12; //0.0001 - 1  **** (-36)dB - 6dB 20logx
    double VolumeRecInDSPCH2 = 12;
    double VolumeRecInDSPCH3 = 12;
    double VolumeRecInDSPCH4 = 12;
    double VolumeRecOutDSPCH1 = 12; //0.0001 - 1  **** (-36)dB - 6dB 20logx
    double VolumeRecOutDSPCH2 = 12;
    double VolumeRecOutDSPCH3 = 12;
    double VolumeRecOutDSPCH4 = 12;


    void codecDSPinit();
    void gpioInit();
    void DSPBootSelect(const bool qspiflash);
    void updateDSPOutputGain(const uint8_t value, const uint8_t outputChannel);
    void backlightOn(){backlight->setValue(false);}
    void backlightOff(){backlight->setValue(true);}
    void set_lna_1_disable(){lna_1_enable->setValue(true);}
    void set_lna_1_enable(){lna_1_enable->setValue(false);}
    void set_lna_2_disable(){lna_2_enable->setValue(true);}
    void set_lna_2_enable(){lna_2_enable->setValue(false);}


    void updateDSPRecInputGain(int value, uint8_t softPhoneID);
    void updateDSPRecOutputGain(int value, uint8_t softPhoneID);
    void updateDSPSpeakerOutputGain(int value, uint8_t softPhoneID);

    double FIRfilter_stateON_INPUT[MOD_FIR1_COUNT] = {0.000389798143361774,0.000651936536801578,0.000993980344817791,
                                                      0.00144776801873279,0.00201772694241966,0.00267145045675678,
                                                      0.00333419990397108,0.0038884561268265,0.00417911130504812,
                                                      0.00402425886959341,0.00323087587718846,0.00161407392739293,
                                                      -0.000981905870412569,-0.00466309957113855,-0.00946787292219549,
                                                      -0.0153537202511853,-0.022190531275701,-0.0297614666016229,
                                                      -0.0377718566433368,-0.045865741450005,-0.0536488839932374,
                                                      -0.0607164008399475,-0.0666826369618758,-0.0712106236774104,
                                                      -0.0740384345617106,0.925,-0.0740384345617106,
                                                      -0.0712106236774104,-0.0666826369618758,-0.0607164008399475,
                                                      -0.0536488839932374,-0.045865741450005,-0.0377718566433368,
                                                      -0.0297614666016229,-0.022190531275701,-0.0153537202511853,
                                                      -0.00946787292219549,-0.00466309957113855,-0.000981905870412569,
                                                      0.00161407392739293,0.00323087587718846,0.00402425886959341,
                                                      0.00417911130504812,0.0038884561268265,0.00333419990397107,
                                                      0.00267145045675678,0.00201772694241966,0.00144776801873279,
                                                      0.00099398034481779,0.000651936536801578,0.000389798143361774};

    double FIRfilter_stateOFF_INPUT[MOD_FIR1_COUNT] = {0.0,0.0,0.0,
                                                       0.0,0.0,0.0,
                                                       0.0,0.0,0.0,
                                                       0.0,0.0,0.0,
                                                       0.0,0.0,0.0,
                                                       0.0,0.0,0.0,
                                                       0.0,0.0,0.0,
                                                       0.0,0.0,0.0,
                                                       0.0,0.0,0.0,
                                                       0.0,0.0,0.0,
                                                       0.0,0.0,0.0,
                                                       0.0,0.0,0.0,
                                                       0.0,0.0,0.0,
                                                       0.0,0.0,0.0,
                                                       0.0,0.0,0.0,
                                                       0.0,0.0,0.0,
                                                       0.0,0.0,1.0};


    double FIRfilter_stateON_OUTPUT[MOD_FIR2_COUNT] = {0.000389798143361774,0.000651936536801578,0.000993980344817791,
                                                       0.00144776801873279,0.00201772694241966,0.00267145045675678,
                                                       0.00333419990397108,0.0038884561268265,0.00417911130504812,
                                                       0.00402425886959341,0.00323087587718846,0.00161407392739293,
                                                       -0.000981905870412569,-0.00466309957113855,-0.00946787292219549,
                                                       -0.0153537202511853,-0.022190531275701,-0.0297614666016229,
                                                       -0.0377718566433368,-0.045865741450005,-0.0536488839932374,
                                                       -0.0607164008399475,-0.0666826369618758,-0.0712106236774104,
                                                       -0.0740384345617106,0.925,-0.0740384345617106,
                                                       -0.0712106236774104,-0.0666826369618758,-0.0607164008399475,
                                                       -0.0536488839932374,-0.045865741450005,-0.0377718566433368,
                                                       -0.0297614666016229,-0.022190531275701,-0.0153537202511853,
                                                       -0.00946787292219549,-0.00466309957113855,-0.000981905870412569,
                                                       0.00161407392739293,0.00323087587718846,0.00402425886959341,
                                                       0.00417911130504812,0.0038884561268265,0.00333419990397107,
                                                       0.00267145045675678,0.00201772694241966,0.00144776801873279,
                                                       0.00099398034481779,0.000651936536801578,0.000389798143361774};
    double FIRfilter_stateOFF_OUTPUT[MOD_FIR2_COUNT]={0.0,0.0,0.0,
                                                        0.0,0.0,0.0,
                                                        0.0,0.0,0.0,
                                                        0.0,0.0,0.0,
                                                        0.0,0.0,0.0,
                                                        0.0,0.0,0.0,
                                                        0.0,0.0,0.0,
                                                        0.0,0.0,0.0,
                                                        0.0,0.0,0.0,
                                                        0.0,0.0,0.0,
                                                        0.0,0.0,0.0,
                                                        0.0,0.0,0.0,
                                                        0.0,0.0,0.0,
                                                        0.0,0.0,0.0,
                                                        0.0,0.0,0.0,
                                                        0.0,0.0,0.0,
                                                        0.0,0.0,1.0};
    static void* ThreadFuncSqlWatcher(void* pTr);
    typedef void * (*THREADFUNCPTRSQLWATCHER)(void *);
    pthread_t idThreadSqlWatcher;
    std::atomic_bool m_threadRunning{true};
    QString m_lastRecState;     // เช่น "RECORD", "PAUSE"
    bool m_lastRecIsRecord = false;
private slots:
    void onSetFreqDone(quint64 freqHz, bool ok);
    void startScanCardFn();
    void updateGPIOKeyProfilesSlot(int code);
    void updateRotaryProfilesSlot(int dir);
    void newCommandProcess(const QJsonObject command,QWebSocket *pSender, const QString& message);
    void newCommandProcessWeb(const QJsonObject command,QWebSocket *pSender, const QString& message);
    // void newCommandProcess(QJsonObject command, QWebSocket *pSender, QString message);
    void socketClientReconnect();
    void openwebrxConnected();
    void onSQLChanged(bool sqlVal);
    void fileUpdated(const QString &path);
    void onNewClientConneced(QWebSocket *socketClient);
    void onNewClientConneceds(QWebSocket *socketClient);
    void sendSquelchStatus(bool sqlVal);
    void newSettingPageConnectd(QWebSocket *pSender);

};

#endif // MAINWINDOWS_H
