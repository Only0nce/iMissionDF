/****************************************************************************
  **
  ** Copyright (C) 2016 Kurt Pattyn <pattyn.kurt@gmail.com>.
  ** Contact: https://www.qt.io/licensing/
  **
  ** This file is part of the QtWebSockets module of the Qt Toolkit.
  **
  ** $QT_BEGIN_LICENSE:BSD$
  ** Commercial License Usage
  ** Licensees holding valid commercial Qt licenses may use this file in
  ** accordance with the commercial license agreement provided with the
  ** Software or, alternatively, in accordance with the terms contained in
  ** a written agreement between you and The Qt Company. For licensing terms
  ** and conditions see https://www.qt.io/terms-conditions. For further
  ** information use the contact form at https://www.qt.io/contact-us.
  **
  ** BSD License Usage
  ** Alternatively, you may use this file under the terms of the BSD license
  ** as follows:
  **
  ** "Redistribution and use in source and binary forms, with or without
  ** modification, are permitted provided that the following conditions are
  ** met:
  **   * Redistributions of source code must retain the above copyright
  **     notice, this list of conditions and the following disclaimer.
  **   * Redistributions in binary form must reproduce the above copyright
  **     notice, this list of conditions and the following disclaimer in
  **     the documentation and/or other materials provided with the
  **     distribution.
  **   * Neither the name of The Qt Company Ltd nor the names of its
  **     contributors may be used to endorse or promote products derived
  **     from this software without specific prior written permission.
  **
  **
  ** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  ** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  ** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  ** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  ** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  ** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  ** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  ** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  ** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  ** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
  **
  ** $QT_END_LICENSE$
  **
  ****************************************************************************/
#ifndef CHATIGATESERVER_H
#define CHATIGATESERVER_H


#include <QObject>
#include <QList>
#include <QByteArray>
#include "QWebSocketServer"
#include "QWebSocket"
#include <QDebug>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonDocument>
#include <QFile>
#include <QString>
#include <QTextStream>

QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)

class ChatiGateServer : public QObject
{
    Q_OBJECT
public:
    explicit ChatiGateServer(quint16 port, QObject *parent = Q_NULLPTR);
    virtual ~ChatiGateServer();
    void broadcastMessage(QString message);
    void sendMessage(QWebSocket *SoftPhoneClient,QString message);
    void sendToIGateGroupMNGClient(QString message,int softPhoneID);
    void updateHomeInput(QString deviceName, QString sipUser, uint8_t inputLevel, uint8_t outputLevel, QWebSocket *newClient, uint16_t keepAlivePeroid, uint16_t sipPort, uint8_t portInterface, uint8_t txScheduler, uint8_t invitemode, float sidetone, int defaultEthernet, int testModeEnable, int softPhoneID, bool mainRadioTransmitterUsed,
                         bool mainRadioReceiverUsed, bool radioAutoInactive, int radioMainStandby, float localSidetone, int pttDelay, uint8_t outputDSPgain, bool state, float Frequency ,int Phase , float Level,
                         uint8_t recInputDSPLevel, uint8_t recOutputDSPLevel, QString recServerAddr1, QString recServerAddr2);

    void updateToneDataClient(bool state, float Frequency ,int Phase , float Level,int softPhoneID,QWebSocket *newClient);
    void updateHomeED137Cfg(uint8_t nodeID, uint8_t nodeType, QString nodeName, QString ipAddress, QString sipPort, uint8_t active, QWebSocket *newClient, QString frequency, QString sqlLevel, QString nodeNameRx, QString ipAddressRx, QString sipPortRx, QString frequencyRx, QString sqlLevelRx, QString channelName, int rfPower);
    void updatefrequency(uint8_t nodeID, QString frequency, QString sqlLevel, QString frequencyRx, int rfPower);
    void updateNetworkInfo(QString dhcpmethod, QString ipaddress, QString subnet, QString gateway, QString pridns, QString secdns, QString ntpServer, QWebSocket *newClient, QString phyNetworkName );
    void updateSystemInfo(QString SwVersion, QString HwVersion, bool ntp, QString ntpServer, QString timeLocation, QWebSocket *newClient);
    void updateHomeNodeState(uint8_t nodeID, QString conn, QString duration, QString durationRx, QString trxStatus, QWebSocket *newClient, int softPhoneID);
    void updateAllowedUri(uint8_t softPhoneID, uint8_t numConn, QString uri1, QString uri2, QString uri3, QString uri4, QString uri5, QString uri6, QString uri7, QString uri8);
    void updateAllowedUri(uint8_t softPhoneID, uint8_t numConn, QString uri1, QString uri2, QString uri3, QString uri4, QString uri5, QString uri6, QString uri7, QString uri8, QWebSocket *newClient);
    void updateClientConnStatus(uint8_t connNum, QString TxRxStatus, QString lastPtt, QWebSocket *newClient, int softPhoneID);
    void broadcastMessageNodeState(uint8_t nodeID, QString conn, QString duration, QString trxStatus, QString radioStatus, QString vswr, QString durationRx, QString connRx, QString radioStatusRx, int softPhoneID);
    void broadcastVUMeter(double in1,double in2,double in3,double in4,double out1,double out2,double out3,double out4, double in1dB,double in2dB,double in3dB,double in4dB,double out1dB,double out2dB,double out3dB,double out4dB,double inOut1,double inOut2,double inOut3,double inOut4,double inOut1dB,double inOut2dB,double inOut3dB,double inOut4dB);
    void broadcastMessageStatus(uint8_t connNum, QString TxRxStatus, QString lastPtt, int softPhoneID);
    void broadcastStatusToRecServer(uint8_t connNum, bool RxStatus, bool TxStatus, QString uriPtt, int softPhoneID);
    void broadcastGpioInStatus(uint8_t gpio1Val, uint8_t gpio2Val, uint8_t gpio3Val, uint8_t gpio4Val, uint8_t gpio1OutVal, uint8_t gpio2OutVal, uint8_t gpio3OutVal, uint8_t gpio4OutVal);
    void updateGpioInStatus(uint8_t gpio1Val,uint8_t gpio2Val, uint8_t gpio3Val, uint8_t gpio4Val,uint8_t gpio1OutVal,uint8_t gpio2OutVal, uint8_t gpio3OutVal, uint8_t gpio4OutVal,QWebSocket *newClient);
    void updateURIConnList(QString uriConnList, QWebSocket *newClient);
    void updateRadioController(int softPhoneID, int RadioController, QWebSocket *newClient);
    void updateSqlActiveHigh(int active, QWebSocket *newClient);
    void updateSqlDefeat(int sqlDefeat, QWebSocket *newClient);
    void broadcastURIConnList(QString uriConnList, int softPhoneID);
    void broadcastConnDuration(QString uri, QString duration, int softPhoneID);
    void broadcastSystemInfo(QString memmory, QString storage, QString cupusage, QString currentTime, QString currentDate);
    void broadcastSystemDateTime(float cputmp);
    void broadcastSystemMessage(QString nodeSelected);
    void broadcastAudioInfo(qreal audioInLevel, qreal audioOutLevel);
    void broadcastTestmodeEnable(int testModeIndex);
    void updateCATIS(bool enableRecording, int warningAudioLevelTime, int warningPercentFault, int warningPTTMinute, int backupAudioMin, QWebSocket *newClient);
    int clientNum = 0;

    QList<QWebSocket *> m_WebSocketVUClients;
    QList<QWebSocket *> m_WebSocketClients;
    QList<QWebSocket *> m_WebSocketRecClients;

    void updateNetworktime(int timeout ,QWebSocket *newClient);
    void updateFIRfiltersendClient(int firinput,int firoutput ,QWebSocket *newClient);
    void sendMessageToRecService(QString message, int softPhoneID);
    void sendMessageToSoftPhone(QString message, int softPhoneID);
    void updateRadioTestDataClient(bool state, bool ptt, int timeout, float Frequency ,int Phase , float Level,int softPhoneID,QWebSocket *newClient);

signals:
    void handleApplyRecSettings(const QJsonObject &obj);
    void recLogging(int softPhoneID, int recorderID,QString recState, QString message);
    void newConnection(QString menuIndex, QWebSocket *newClient);
    void updateTRxConfig(int trxID, QString CallName, QString Address, QString sipPort, QString trxMode, QString active, QString CallNameRx, QString AddressRx, QString sipPortRx, QString channelName);
    void updateNetwork(uint8_t dhcpmethod, QString ipaddress, QString subnet, QString gateway, QString pridns, QString secdns, QString phyNetworkName);
    void updateHostConfig(QString sipuser, int sipport, int keepaliveperoid, QString defaultEthernet, int softPhoneID);
    void reconnectTRx(int trxID);

    void updateswitchInviteMode(int value);
    void onLocalSidetoneLoopbackChanged(bool localSidetoneLoopbackOn,int softPhoneID);
    void updateSiteTone(float value,int softPhoneID);
    void updateGroupMute(uint8_t value,int softPhoneID);
    void updatePttDelay(uint8_t value,int softPhoneID);
    void updateLocalSiteTone(float value,int softPhoneID);
    void updatePortInterface(int value);
    void updatetxScheduler(int value);
    void disconnectUri(QString value);
    void updateFirmware();
    void toggleGpioOut(int gpioNum,int gpioVal);
    void updateNTPServer(QString value);
    void setcurrentDatetime(QString value);
    void setLocation(QString value);
    void restartnetwork();
    void rebootSystem();
    void updateInputGain(uint8_t value, uint8_t softPhoneID);
    void updateOutputGain(uint8_t value, uint8_t softPhoneID);
    void updateDSPOutputGain(int value, uint8_t softPhoneID);
    void updateDSPRecInputGain(int value, uint8_t softPhoneID);
    void updateDSPRecOutputGain(int value, uint8_t softPhoneID);
    void updateRecAddress(QString value,int index ,uint8_t softPhoneID);
    // TONE
    void updateInputTone(bool state, float Frequency ,int Phase , float Level , uint8_t softPhoneID);
    void updateServerTone(bool tonestate, bool pttstate,int timeoutptt, float ServerFrequency, int ServerPhase, float ServerLevel, uint8_t softPhoneID);
    void getTestRadioPage(QWebSocket *pSender);
    void getRedioTestHomePage(uint8_t softPhoneID,QWebSocket *pSender);
    void EventDataLogger(const QJsonObject &messageObj);

    void updateFIRfilter(bool firinput,bool firoutput);

    void updateNetworkTimeout(int timeout);

    void updateRxBestSignalEnable(bool val, int softPhoneID);
    void updateURILits(QStringList urlList, int softPhoneID);
    void updateNumconn(int numconn, int softPhoneID);
    void updateSQLDefeat(bool sqlDefeat, int softPhoneID);
    void updateSQLActiveHigh(bool sqlActive, int softPhoneID);
    void updateRadioAutoInactive(bool value, int softPhoneID);
    void updateMainRadioReceiverUsed(bool value, int softPhoneID);
    void updateMainRadioTransmitterUsed(bool value, int softPhoneID);
    void setRadioController(int RadioController, int softPhoneID);
    void updateRadioMainStandby(int value, int softPhoneID);
    void updateRadioActive(bool radioAutoInactive,bool txMainRadioUsed,bool rxMainRadioUsed, int radioMainStandby, int softPhoneID);
    void updateTestmode(int testModeEnable);
    void onNumClientChanged(int numclient);
    void updateCATISCONF(QString enableRecording, int warningAudioLevelTime, int warningPercentFault, int warningPTTMinute, int backupAudioMin);
    void newSoftphone(int softphoneID, QWebSocket *newClient);
    void removeSoftphone(QWebSocket *newClient);

    void addNewRadio(QString radioName, QString trxMode, QString uri, QString ipAddress, int r2sPeriod,int sipPort, QString frequenicy);
    bool updateRadio(QString radioName, QString trxMode, QString uri, QString ipAddress, int r2sPeriod, int sipPort, QString frequency, int radioID);
    bool removeRadio(int ID);

    bool addNewChannel(QString channelName, QString mainRadioType, int mainRadio01ID, int mainRadio02ID, bool standbyEnable, QString standbyRadioType, int standbyRadio01ID, int standbyRadio02ID, QString mainRadioName, QString standbyRadioName);
    bool updateChannel(QString channelName, QString mainRadioType, int mainRadio01ID, int mainRadio02ID, bool standbyEnable, QString standbyRadioType, int standbyRadio01ID, int standbyRadio02ID, QString mainRadioName, QString standbyRadioName, int channelID);
    bool removeChannel(int channelID);

    bool updateChannelTxRxEnable(bool mainRadioTransmitterUsed, bool mainRadioReceiverUsed, int softPhoneID);
    bool updateControler(QString sipUser, int keepAlivePeroid, int sipPort, int rtpStartPort, int softPhoneID, int defaultEthernet);
    bool updateChannelID(int channelID, int softPhoneID);
    bool updateRelayOutputMode(uint8_t RelayOutputMode, int softPhoneID);


    void sipStarted(int softPhoneID);
    void onNewMessage(QString message);
    void appendChannelData(uint8_t softPhoneID, int trxMode, bool mainTxPTTOn, bool mainRxSQLOn, int numConn, int numTxRx , QString trxType, QString main01IPAddr, QString main02IPAddr, QString standby01IPAddr, QString standby02IPAddr, double freqMHz, int pttInput_count);
    void appendClientConnStatus(int softPhoneID, QString lastPttOn, QString trxStatus, QString pttURI, bool mainTxPTTOn, bool mainRxSQLOn, int numConn);
    void onSendSquelchStatus(int softPhoneID, bool pttOn, bool sqlOn, bool callState, double freq);
    void appendClientCallInStatus(int softPhoneID, QString uri, QString duration, bool pttOn, bool sqlOn, bool callState);
    void getHomePage(uint8_t softPhoneID,QWebSocket *pSender);
    void getVuMeter(QWebSocket *pSender);
    void getServerHomePage(uint8_t softPhoneID,QWebSocket *pSender);
    void getIGateGroupConfig(uint8_t softPhoneID,QWebSocket *pSender);
    void addIGateGroupMng(int softPhoneID,int iGateID,QString ipaddress,QString uriName);
    void syncIGateGroup(QString toIPAddress,int softPhoneID,int iGateID,QString ipaddress,QString uriName);
    void deleteIGate(int softPhoneID, int id, int iGateID, QString ipaddress);
    void clearIGateGroup(int softPhoneID);
    void deleteIGateGroup(QString toIPAddress,int softPhoneID);
    void syncDeleteIGateAnotherGroup(int deleteIGateID, int softPhoneID, QString ipaddress0, QString ipaddress1);
    void getMonitorPage(QWebSocket *pSender);
    void getNetworkPage(QWebSocket *pSender);
    void getSystemPage(QWebSocket *pSender);
    void getCtrlInfoPage(int softPhoneID);
    void toggleRxEnable(int softPhoneID);
    void toggleTxEnable(int softPhoneID);
    void restartSoftphone(int softPhoneID);
    void updateDeviceName(int softPhoneID,QString deviceName);

    void adcValue(int channel, double value, QString message);
    void systemrestore();
    void systembackup();
public Q_SLOTS :
    void sendSquelchStatus(int softPhoneID, bool pttOn, bool sqlOn, bool callState, QString state, double freq);
    void sendToWebMessageClient(QString message);
    void sendToWebMessageClientWebSender(QString message,QWebSocket *webClient);
private Q_SLOTS:
    void onNewConnection();
    void commandProcess(QString message, QWebSocket *pSender);
    void processMessage(QString message);
    void socketDisconnected();

private:
    struct SoftPhoneSocketClient
    {
        int softPhoneID;
        QWebSocket *SocketClients;
    };

    QList<SoftPhoneSocketClient *> softPhoneSocketClient;
    QList<SoftPhoneSocketClient *> recSocketClient;
    QList<SoftPhoneSocketClient *> igateGroupMngSocketClient;
    QWebSocketServer *m_pWebSocketServer;
    QList<QWebSocket *> m_clients;


    QList<QWebSocket *> m_iGateGroupMngWebSocketClients;
    QWebSocket *newSocket;
    int lasthwClockUpdate = 0;
    QString getUPTime();
    QString readLine(QString fileName);
};

#endif //ChatiGateServer_H
