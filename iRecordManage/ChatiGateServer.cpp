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
#include "ChatiGateServer.h"
#include <QDateTime>

#define EMER            "EMR"
#define RPTON           "RPT"
#define PTTON           "Tx"
#define RXON            "Rx"
#define PTTRX           "Tx&Rx"
#define STANDBY         "--"
#define WAIT            "***"

QT_USE_NAMESPACE

ChatiGateServer::ChatiGateServer(quint16 port, QObject *parent) :
    QObject(parent),
    m_pWebSocketServer(Q_NULLPTR),
    m_clients()
{
    m_pWebSocketServer = new QWebSocketServer(QStringLiteral("Chat Server"),
                                              QWebSocketServer::NonSecureMode,
                                              this);
    if (m_pWebSocketServer->listen(QHostAddress::Any, port))
    {
        qDebug() << "Chat Server listening on port" << port;
        connect(m_pWebSocketServer, &QWebSocketServer::newConnection,
                this, &ChatiGateServer::onNewConnection);
    }
}

ChatiGateServer::~ChatiGateServer()
{
    m_pWebSocketServer->close();
    qDeleteAll(m_clients.begin(), m_clients.end());
}

void ChatiGateServer::onNewConnection()
{
    QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();
    connect(pSocket, &QWebSocket::textMessageReceived, this, &ChatiGateServer::processMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &ChatiGateServer::socketDisconnected);
    m_clients << pSocket;
    qDebug() << "On New Connection from address : " << pSocket->peerName();
    if (clientNum <= 0)
    {
        clientNum = m_clients.length();
        emit onNumClientChanged(clientNum);
    }
    else {
        clientNum = m_clients.length();
    }

}
//var obj = JSON.parse('{"menuID":"input", "name":"S4_1", "inputgain":1, "outputgain":10}');
//var obj = JSON.parse('{"menuID":"nodeCfg", "nodeID":2, "nodeType":0, "nodeName":"t6tr1", "ipAddress":"10.45.110.11", "sipPort":"5060", "active":1}');
void ChatiGateServer::updateHomeInput(QString deviceName, QString sipUser, uint8_t inputLevel, uint8_t outputLevel, QWebSocket *newClient, uint16_t keepAlivePeroid, uint16_t sipPort, uint8_t portInterface ,uint8_t txScheduler,uint8_t invitemode,float sidetone, int defaultEthernet, int testModeEnable, int softPhoneID,bool mainRadioTransmitterUsed,bool mainRadioReceiverUsed, bool radioAutoInactive, int radioMainStandby, float localSidetone,
                                      int pttDelay, uint8_t outputDSPgain,bool state, float Frequency ,int Phase , float Level,
                                      uint8_t recInputDSPLevel, uint8_t recOutputDSPLevel, QString recServerAddr1, QString recServerAddr2)
{
    /*bool state, float Frequency ,int Phase , int Level*/
    QString message = QString("{\"menuID\":\"input\", \"sipUser\":\"%1\", \"sipPort\":\"%2\", \"keepAlivePeroid\":\"%3\", \"inputgain\":%4, \"outputgain\":%5, \"portInterface\":%6, \"pttScheduler\":%7, "
                              "\"invitemode\":%8, \"sidetone\":%9, \"defaultEthernet\":%10, \"testModeEnable\":%11, \"deviceName\":\"%12\", \"softPhoneID\":%13, \"mainRadioTransmitterUsed\":%14, "
                              "\"mainRadioReceiverUsed\":%15, \"radioAutoInactive\":%16, \"radioMainStandby\":%17, \"localSidetone\":%18, \"pttDelay\":%19, \"outputDSPgain\":%20,"
                              "\"outputToneState\":%21, \"outputToneFrequency\":%22, \"outputTonePhase\":%23, \"outputToneLevel\":%24, "
                              "\"recInputDSPLevel\":%25, \"recOutputDSPLevel\":%26, \"recServerAddr1\":\"%27\", \"recServerAddr2\":\"%28\" "
                              "}")
                          .arg(sipUser).arg(sipPort).arg(keepAlivePeroid).arg(inputLevel).arg(outputLevel).arg(portInterface).arg(txScheduler).arg(invitemode).arg(int(sidetone*10)/2).arg(defaultEthernet)
                          .arg(testModeEnable).arg(deviceName).arg(softPhoneID).arg(mainRadioTransmitterUsed).arg(mainRadioReceiverUsed).arg(radioAutoInactive).arg(radioMainStandby).arg(localSidetone).arg(pttDelay)
                          .arg(outputDSPgain) .arg(state).arg(Frequency).arg(Phase).arg(Level)
                          .arg (recInputDSPLevel).arg(recOutputDSPLevel).arg(recServerAddr1).arg(recServerAddr2);
    newClient->sendTextMessage(message);
}

void ChatiGateServer::updateToneDataClient(bool state, float Frequency ,int Phase , float Level,int softPhoneID,QWebSocket *newClient){
    QString message = QString("{\"menuID\":\"updateInputTone\",\"outputToneState\":%1, \"outputToneFrequency\":%2, \"outputTonePhase\":%3, \"outputToneLevel\":%4}")
    .arg(state).arg(Frequency).arg(Phase).arg(Level).arg(softPhoneID);
    newClient->sendTextMessage(message);
}
void ChatiGateServer::updateRadioTestDataClient(bool state, bool ptt, int timeout, float Frequency ,int Phase , float Level,int softPhoneID,QWebSocket *newClient){
    QString message = QString(
                          "{\"menuID\":\"UpdateToneServer\",\"UpdateToneServerState\":\"%1\","
                          "\"pttState\":\"%2\",\"ToneServerFrequency\":\"%3\","
                          "\"ToneServerPhase\":\"%4\",\"ToneServerLevel\":\"%5\",\"timeoutptt\":\"%6\","
                          "\"softPhoneID\":\"%7\"}")
                          .arg(state)
                          .arg(ptt)
                          .arg(Frequency)
                          .arg(Phase)
                          .arg(Level)
                          .arg(timeout)
                          .arg(softPhoneID);
    newClient->sendTextMessage(message);
}
void ChatiGateServer::updateCATIS(bool enableRecording, int warningAudioLevelTime, int warningPercentFault, int warningPTTMinute, int backupAudioMin, QWebSocket *newClient)
{
    QString message = QString("{\"menuID\":\"updateCATIS\", \"enableRecording\":%1, \"warningAudioLevelTime\":%2, \"warningPercentFault\":%3, \"warningPTTMinute\":%4, \"backupAudioMin\":%5}")
    .arg(enableRecording).arg(warningAudioLevelTime).arg(warningPercentFault).arg(warningPTTMinute).arg(backupAudioMin);
    newClient->sendTextMessage(message);
}

void ChatiGateServer::updatefrequency(uint8_t nodeID, QString frequency, QString sqlLevel,QString frequencyRx, int rfPower)
{
    QString message = QString("{\"menuID\":\"updatefrequency\", \"nodeID\":%1, \"frequency\":\"%2\", \"sqlLevel\":\"%3\", \"frequencyRx\":\"%4\", \"rfPower\":%5}").arg(nodeID).arg(frequency).arg(sqlLevel).arg(frequencyRx).arg(rfPower);
    broadcastMessage(message);
}

void ChatiGateServer::updateHomeED137Cfg(uint8_t nodeID, uint8_t nodeType, QString nodeName, QString ipAddress, QString sipPort, uint8_t active, QWebSocket *newClient, QString frequency, QString sqlLevel,
                                         QString nodeNameRx, QString ipAddressRx, QString sipPortRx, QString frequencyRx, QString sqlLevelRx, QString channelName, int rfPower)
{
    QString message = QString("{\"menuID\":\"nodeCfg\", \"nodeID\":%1, \"nodeType\":%2, \"nodeName\":\"%3\", \"ipAddress\":\"%4\", \"sipPort\":\"%5\", \"active\":%6, \"frequency\":\"%7\", \"sqlLevel\":\"%8\""
                              ", \"nodeNameRx\":\"%9\", \"ipAddressRx\":\"%10\", \"sipPortRx\":\"%11\", \"frequencyRx\":\"%12\", \"sqlLevelRx\":\"%13\", \"channelName\":\"%14\", \"rfPower\":%15}"
                              ).arg(nodeID).arg(nodeType).arg(nodeName).arg(ipAddress).arg(sipPort).arg(active).arg(frequency).arg(sqlLevel).arg(nodeNameRx).arg(ipAddressRx).arg(sipPortRx).arg(frequencyRx).arg(sqlLevelRx).arg(channelName).arg(rfPower);
    newClient->sendTextMessage(message);
}

void ChatiGateServer::updateHomeNodeState(uint8_t nodeID, QString conn, QString duration, QString durationRx, QString trxStatus, QWebSocket *newClient, int softPhoneID)
{
    QString message = QString("{\"menuID\":\"connState\", \"nodeID\":%1, \"connStatus\":\"%2\", \"connDuration\":\"%3\", \"connDuration\":\"%4\", \"trxStatus\":\"%5\", \"trxStatus\":%6, \"softPhoneID\":%7}")
    .arg(nodeID).arg(conn).arg(duration).arg(durationRx).arg(trxStatus).arg(softPhoneID);
    newClient->sendTextMessage(message);
}
void ChatiGateServer::updateAllowedUri(uint8_t softPhoneID, uint8_t numConn, QString uri1, QString uri2, QString uri3, QString uri4, QString uri5, QString uri6, QString uri7, QString uri8, QWebSocket *newClient)
{
    QString message = QString("{\"menuID\":\"uriAllowedList\", \"numConn\":%1, \"uri1\":\"%2\", \"uri2\":\"%3\", \"uri3\":\"%4\", \"uri4\":\"%5\", \"uri5\":\"%6\", \"uri6\":\"%7\", \"uri7\":\"%8\", \"uri8\":\"%9\", \"softPhoneID\":%10}")
    .arg(numConn).arg(uri1).arg(uri2).arg(uri3).arg(uri4).arg(uri5).arg(uri6).arg(uri7).arg(uri8).arg(softPhoneID);
    newClient->sendTextMessage(message);
}
void ChatiGateServer::updateClientConnStatus(uint8_t connNum, QString TxRxStatus, QString lastPtt, QWebSocket *newClient,int softPhoneID)
{
    QString message = QString("{\"menuID\":\"connStatus\", \"connNum\":%1, \"TxRx\":\"%2\", \"pttURI\":\"%3\", \"softPhoneID\":%4}").arg(connNum).arg(TxRxStatus).arg(lastPtt).arg(softPhoneID);
    newClient->sendTextMessage(message);
}
void ChatiGateServer::updateNetworkInfo(QString dhcpmethod, QString ipaddress, QString subnet, QString gateway, QString pridns, QString secdns, QString ntpServer, QWebSocket *newClient, QString phyNetworkName)
{
    QString message = QString("{\"menuID\":\"network\", \"dhcpmethod\":\"%1\", \"ipaddress\":\"%2\", \"subnet\":\"%3\", \"gateway\":\"%4\", \"pridns\":\"%5\", \"secdns\":\"%6\", \"ntpServer\":\"%7\", \"phyNetworkName\":\"%8\"}")
    .arg(dhcpmethod).arg(ipaddress).arg(subnet).arg(gateway).arg(pridns).arg(secdns).arg(ntpServer).arg(phyNetworkName);
    newClient->sendTextMessage(message);
}
void ChatiGateServer::updateNetworktime(int timeout,QWebSocket *newClient){
    QString message = QString("{\"menuID\":\"updateTimeout\",\"NetworkTimeout\":%1}")
    .arg(timeout);
    newClient->sendTextMessage(message);
}

void ChatiGateServer::updateSystemInfo(QString SwVersion, QString HwVersion,bool ntp,QString ntpServer,QString timeLocation,QWebSocket *newClient)
{
    int dateTimeMethod;
    if (ntp){
        dateTimeMethod = 1;
    }else{
        dateTimeMethod = 2;
    }

    QString message = QString("{\"menuID\":\"system\", \"SwVersion\":\"%1\", \"HwVersion\":\"%2\", \"dateTimeMethod\":\"%3\", \"ntpServer\":\"%4\", \"location\":\"%5\"}")
                          .arg(SwVersion).arg(HwVersion).arg(dateTimeMethod).arg(ntpServer).arg(timeLocation);
    newClient->sendTextMessage(message);
}

void ChatiGateServer::broadcastMessageNodeState(uint8_t nodeID, QString conn, QString duration, QString trxStatus, QString radioStatus, QString vswr, QString durationRx, QString connRx, QString radioStatusRx, int softPhoneID)
{
    QString message = QString("{\"menuID\":\"connState\", \"nodeID\":%1, \"connStatus\":\"%2\", \"connDuration\":\"%3\", \"trxStatus\":\"%4\", \"radioStatus\":\"%5\", \"vswr\":\"%6\", \"connDurationRx\":\"%7\", \"connStatusRx\":\"%8\", \"radioStatusRx\":\"%9\", \"softPhoneID\":%10}")
    .arg(nodeID).arg(conn).arg(duration).arg(trxStatus).arg(radioStatus).arg(vswr).arg(durationRx).arg(connRx).arg(radioStatusRx).arg(softPhoneID);
    Q_FOREACH (QWebSocket *pClient, m_WebSocketClients)
    {
        pClient->sendTextMessage(message);
    }
}
void ChatiGateServer::broadcastVUMeter(double in1,double in2,double in3,double in4,double out1,double out2,double out3,double out4, double in1dB,double in2dB,double in3dB,double in4dB,double out1dB,double out2dB,double out3dB,double out4dB,
                                       double inOut1,double inOut2,double inOut3,double inOut4,double inOut1dB,double inOut2dB,double inOut3dB,double inOut4dB)
{
    QString message = QString("{\"menuID\":\"broadcastVUMeter\", "
                              "\"in1\"  :%1, "
                              "\"in2\"  :%2, "
                              "\"in3\"  :%3, "
                              "\"in4\"  :%4, "
                              "\"in1dB\":%5, "
                              "\"in2dB\":%6, "
                              "\"in3dB\":%7, "
                              "\"in4dB\":%8, "
                              "\"out1\" :%9, "
                              "\"out2\" :%10, "
                              "\"out3\" :%11, "
                              "\"out4\" :%12, "
                              "\"out1dB\"   :%13, "
                              "\"out2dB\"   :%14, "
                              "\"out3dB\"   :%15, "
                              "\"out4dB\"   :%16, "
                              "\"inOut1\" :%17, "
                              "\"inOut2\" :%18, "
                              "\"inOut3\" :%19, "
                              "\"inOut4\" :%20, "
                              "\"inOut1dB\"   :%21, "
                              "\"inOut2dB\"   :%22, "
                              "\"inOut3dB\"   :%23, "
                              "\"inOut4dB\"   :%24 "
                              "}")
                          .arg(in1).arg(in2).arg(in3).arg(in4)
                          .arg(in1dB).arg(in2dB).arg(in3dB).arg(in4dB)
                          .arg(out1).arg(out2).arg(out3).arg(out4)
                          .arg(out1dB).arg(out2dB).arg(out3dB).arg(out4dB)
                          .arg(inOut1).arg(inOut2).arg(inOut3).arg(inOut4)
                          .arg(inOut1dB).arg(inOut2dB).arg(inOut3dB).arg(inOut4dB)
        ;
    Q_FOREACH (QWebSocket *pClient, m_WebSocketVUClients)
    {
        pClient->sendTextMessage(message);
    }

    Q_FOREACH (SoftPhoneSocketClient *pClient, softPhoneSocketClient)
    {
        pClient->SocketClients->sendTextMessage(message);
    }
}
void ChatiGateServer::broadcastMessageStatus(uint8_t connNum, QString TxRxStatus, QString lastPtt, int softPhoneID)
{
    QString message = QString("{\"menuID\":\"connStatus\", \"connNum\":%1, \"TxRx\":\"%2\", \"pttURI\":\"%3\", \"softPhoneID\":%4}").arg(connNum).arg(TxRxStatus).arg(lastPtt).arg(softPhoneID);
    Q_FOREACH (QWebSocket *pClient, m_WebSocketClients)
    {
        pClient->sendTextMessage(message);
    }
}
void ChatiGateServer::broadcastStatusToRecServer(uint8_t connNum, bool RxStatus, bool TxStatus, QString uriPtt, int softPhoneID)
{
    QJsonObject jsonObj;
    jsonObj["menuID"] = "connStatus";
    jsonObj["connNum"] = connNum;
    jsonObj["RxStatus"] = RxStatus;
    jsonObj["TxStatus"] = TxStatus;
    jsonObj["uriPtt"] = uriPtt;
    jsonObj["softPhoneID"] = softPhoneID;
    QJsonDocument jsonDoc(jsonObj);
    QString jsonString = jsonDoc.toJson(QJsonDocument::Compact);
    //    qDebug() << "ChatiGateServer::broadcastStatusToRecServer" << jsonString;
    Q_FOREACH (QWebSocket *pClient, m_WebSocketRecClients)
    {
        pClient->sendTextMessage(jsonString);
    }
}
void ChatiGateServer::updateGpioInStatus(uint8_t gpio1Val, uint8_t gpio2Val, uint8_t gpio3Val, uint8_t gpio4Val, uint8_t gpio1OutVal, uint8_t gpio2OutVal, uint8_t gpio3OutVal, uint8_t gpio4OutVal, QWebSocket *newClient)
{
    QString message = QString("{\"menuID\":\"GpioInStatus\", \"gpio1Val\":%1, \"gpio2Val\":%2, \"gpio3Val\":%3, \"gpio4Val\":%4, \"gpio1OutVal\":%5, \"gpio2OutVal\":%6, \"gpio3OutVal\":%7, \"gpio4OutVal\":%8}")
    .arg(gpio1Val).arg(gpio2Val).arg(gpio3Val).arg(gpio4Val).arg(gpio1OutVal).arg(gpio2OutVal).arg(gpio3OutVal).arg(gpio4OutVal);
    newClient->sendTextMessage(message);
}
void ChatiGateServer::broadcastGpioInStatus(uint8_t gpio1Val,uint8_t gpio2Val, uint8_t gpio3Val, uint8_t gpio4Val,uint8_t gpio1OutVal,uint8_t gpio2OutVal, uint8_t gpio3OutVal, uint8_t gpio4OutVal)
{
    QString message = QString("{\"menuID\":\"GpioInStatus\", \"gpio1Val\":%1, \"gpio2Val\":%2, \"gpio3Val\":%3, \"gpio4Val\":%4, \"gpio1OutVal\":%5, \"gpio2OutVal\":%6, \"gpio3OutVal\":%7, \"gpio4OutVal\":%8}")
    .arg(gpio1Val).arg(gpio2Val).arg(gpio3Val).arg(gpio4Val).arg(gpio1OutVal).arg(gpio2OutVal).arg(gpio3OutVal).arg(gpio4OutVal);
    Q_FOREACH (QWebSocket *pClient, m_WebSocketClients)
    {
        pClient->sendTextMessage(message);
    }
}
void ChatiGateServer::updateRadioController(int softPhoneID, int RadioController, QWebSocket *newClient)
{
    QString message = QString("{\"menuID\":\"updateRadioController\", \"softPhoneID\":%1, \"RadioController\":%2}").arg(softPhoneID).arg(RadioController);
    newClient->sendTextMessage(message);
}
void ChatiGateServer::updateSqlActiveHigh(int active, QWebSocket *newClient)
{
    QString message = QString("{\"menuID\":\"SqlActiveHigh\", \"active\":\"%2\"}").arg(active);
    newClient->sendTextMessage(message);
}
void ChatiGateServer::updateFIRfiltersendClient(int firinput,int firoutput , QWebSocket *newClient)
{
    QString message = QString("{\"menuID\":\"updateFIRfilter\", \"firActiveStateInput\":%1, \"firActiveStateOutput\":%2}").arg(firinput).arg(firoutput);
    newClient->sendTextMessage(message);
}
void ChatiGateServer::updateSqlDefeat(int sqlDefeat, QWebSocket *newClient)
{
    QString message = QString("{\"menuID\":\"sqlDefeat\", \"sqlDefeat\":\"%2\"}").arg(sqlDefeat);
    newClient->sendTextMessage(message);
}
void ChatiGateServer::updateURIConnList(QString uriConnList, QWebSocket *newClient)
{
    QString message = QString("{\"menuID\":\"uriConnList\", \"listURI\":\"%1\"}").arg(uriConnList);
    newClient->sendTextMessage(message);
    qDebug() << "updateURIConnList" << message;
}
void ChatiGateServer::broadcastURIConnList(QString uriConnList, int softPhoneID)
{
    QString message = QString("{\"menuID\":\"uriConnList\", \"listURI\":\"%1\", \"softPhoneID\":%2}").arg(uriConnList).arg(softPhoneID);
    // qDebug() << message;
    Q_FOREACH (QWebSocket *pClient, m_WebSocketClients)
    {
        pClient->sendTextMessage(message);
    }
}
void ChatiGateServer::broadcastConnDuration(QString uri, QString duration, int softPhoneID)
{
    QString message = QString("{\"menuID\":\"ConnDuration\", \"uri\":\"%1\", \"duration\":\"%2\", \"softPhoneID\":%3}").arg(uri).arg(duration).arg(softPhoneID);
    Q_FOREACH (QWebSocket *pClient, m_WebSocketClients)
    {
        pClient->sendTextMessage(message);
    }
}
void ChatiGateServer::broadcastSystemInfo(QString memmory, QString storage, QString cupusage, QString currentTime, QString currentDate)
{
    QString message = QString("{\"menuID\":\"SystemInfo\", \"memmory\":%1, \"storage\":%2, \"cupusage\":%3, \"currentTime\":\"%4\", \"currentDate\":\"%5\"}")
    .arg(memmory).arg(storage).arg(cupusage).arg(currentTime).arg(currentDate);
    Q_FOREACH (QWebSocket *pClient, m_WebSocketClients)
    {
        pClient->sendTextMessage(message);
    }
}
void ChatiGateServer::broadcastAudioInfo(qreal audioInLevel, qreal audioOutLevel)
{
    QString message = QString("{\"menuID\":\"AudioInfo\", \"audioInLevel\":%1, \"audioOutLevel\":%2}")
    .arg(audioInLevel).arg(audioOutLevel);
    Q_FOREACH (QWebSocket *pClient, m_WebSocketClients)
    {
        pClient->sendTextMessage(message);
    }
}
void ChatiGateServer::broadcastTestmodeEnable(int testModeIndex)
{
    QString message = QString("{\"menuID\":\"testModeEnable\", \"testModeIndex\":%1}")
    .arg(testModeIndex);
    Q_FOREACH (QWebSocket *pClient, m_WebSocketClients)
    {
        pClient->sendTextMessage(message);
    }
}
void ChatiGateServer::broadcastSystemMessage(QString nodeSelected)
{
    QString currentTime = QTime::currentTime().toString("hh:mm:ss");
    QString currentDate = QDate::currentDate().toString("dd MMM yyyy");
    QString message = QString("{\"menuID\":\"broadcastLocalTime\", \"currentTime\":\"%1\", \"currentDate\":\"%2\", \"nodeSelected\":\"%3\"}")
                          .arg(currentTime).arg(currentDate).arg(nodeSelected);
    Q_FOREACH (QWebSocket *pClient, m_WebSocketClients)
    {
        pClient->sendTextMessage(message);
    }
}
QString ChatiGateServer::getUPTime()
{
    system("uptime -p > /etc/uptime");
    QString fileName = QString("/etc/uptime");
    return readLine(fileName);
}

QString ChatiGateServer::readLine(QString fileName)
{
    QFile inputFile(fileName);
    inputFile.open(QIODevice::ReadOnly);
    if (!inputFile.isOpen())
        return "";

    QTextStream stream(&inputFile);
    QString line = stream.readLine();
    inputFile.close();
    //    qDebug() << line;
    return line.trimmed();
}

void ChatiGateServer::broadcastSystemDateTime(float cputmp)
{
    QString currentTime = QTime::currentTime().toString("hh:mm:ss");
    QString currentDate = QDate::currentDate().toString("dd MMM yyyy");
    //    system("uptime -p > /etc/uptime");
    //    QString uptimeStr = QString("/etc/uptime");
    QString uptimeStr = getUPTime().replace("up","");
    QString message = QString("{\"menuID\":\"broadcastLocalTime\", \"currentTime\":\"%1\", \"currentDate\":\"%2\", \"cputmp\":\"%3°C\", \"uptime\":\"%4\"}")
                          .arg(currentTime)
                          .arg(currentDate)
                          .arg(QString::number(cputmp, 'f', 2))
                          .arg(uptimeStr);

    int min = QTime::currentTime().minute();
    if ((min%5 == 0) & (lasthwClockUpdate != min)){
        system("hwclock -w > /dev/null 2>&1 &");
        lasthwClockUpdate = min;
    }

    Q_FOREACH (QWebSocket *pClient, m_WebSocketClients)
    {
        pClient->sendTextMessage(message);
    }
}

void ChatiGateServer::broadcastMessage(QString message){
    Q_FOREACH (QWebSocket *pClient, m_clients)
    {
        pClient->sendTextMessage(message);
    }
    Q_FOREACH (QWebSocket *pClient, m_WebSocketClients)
    {
        pClient->sendTextMessage(message);
    }
}
void ChatiGateServer::sendMessage(QWebSocket *SoftPhoneClient,QString message)
{
    Q_FOREACH (QWebSocket *pClient, m_clients)
    {
        if (SoftPhoneClient == pClient)
            pClient->sendTextMessage(message);
    }
}
void ChatiGateServer::sendToWebMessageClient(QString message)
{
    // qDebug() << "sendToWebMessageClient_iGateServer" << message;
    Q_FOREACH (QWebSocket *webClient, m_WebSocketClients)
    {
        webClient->sendTextMessage(message);
        // qDebug() << "webClient:" << message;

    }
    Q_FOREACH (QWebSocket *pClient, m_WebSocketVUClients)
    {
        pClient->sendTextMessage(message);
        // qDebug() << "pClient:" << message;

    }
}
void ChatiGateServer::sendToIGateGroupMNGClient(QString message, int softPhoneID)
{
    Q_FOREACH (SoftPhoneSocketClient *sClient, igateGroupMngSocketClient)
    {
        if (sClient->softPhoneID == softPhoneID)
        {
            sClient->SocketClients->sendTextMessage(message);
            break;
        }
    }
}
void ChatiGateServer::sendToWebMessageClientWebSender(QString message,QWebSocket *webClient)
{
    Q_FOREACH (QWebSocket *client, m_WebSocketClients)
    {
        if (client == webClient)
            webClient->sendTextMessage(message);
    }
}
void ChatiGateServer::updateAllowedUri(uint8_t softPhoneID, uint8_t numConn, QString uri1, QString uri2, QString uri3, QString uri4, QString uri5, QString uri6, QString uri7, QString uri8)
{
    QString message = QString("{\"menuID\":\"uriAllowedList\", \"numConn\":%1, \"uri1\":\"%2\", \"uri2\":\"%3\", \"uri3\":\"%4\", \"uri4\":\"%5\", \"uri5\":\"%6\", \"uri6\":\"%7\", \"uri7\":\"%8\", \"uri8\":\"%9\", \"softPhoneID\":%10}")
    .arg(numConn).arg(uri1).arg(uri2).arg(uri3).arg(uri4).arg(uri5).arg(uri6).arg(uri7).arg(uri8).arg(softPhoneID);
    Q_FOREACH (QWebSocket *client, m_WebSocketClients)
    {
        client->sendTextMessage(message);
    }
}

void ChatiGateServer::sendSquelchStatus(int softPhoneID, bool pttOn, bool sqlOn, bool callState, QString state, double freq)
{
    QJsonObject message;
    message["iGateID"] = softPhoneID;
    message["object"] = "receiverStatus";
    message["state"] = state;
    message["squelch"] = sqlOn ? "on" : "off";
    message["pttOn"] = pttOn ? "on" : "off";
    message["device"] = QString("recin%1").arg(softPhoneID <=4 ? softPhoneID : softPhoneID-4);
    message["recorder"] = "enable";
    message["frequency"] = freq*1e6;

    QJsonDocument doc(message);
    QString jsonString = doc.toJson(QJsonDocument::Compact);
    qDebug() <<"sendSquelchStatus:jsonString_GateServer" << jsonString << softPhoneID;
    sendMessageToRecService(jsonString,softPhoneID);
}

void ChatiGateServer::sendMessageToRecService(QString message, int softPhoneID)
{
    qDebug() << "sendMessageToRecService:" << message << softPhoneID << recSocketClient;

    Q_FOREACH (SoftPhoneSocketClient *sClient, recSocketClient)
    {
        if (!sClient || !sClient->SocketClients) {
            qWarning() << "sendMessageToRecService: skip null client";
            continue;
        }

        qDebug() <<  "debug1" << "softPhoneID in list =" << sClient->softPhoneID;
        if (sClient->softPhoneID == softPhoneID)
        {
            qDebug() <<  "debug2" << "sending to client";
            sClient->SocketClients->sendTextMessage(message);
        }
    }
}

void ChatiGateServer::sendMessageToSoftPhone(QString message, int softPhoneID)
{
    Q_FOREACH (SoftPhoneSocketClient *sClient, softPhoneSocketClient)
    {
        if (sClient->softPhoneID == softPhoneID)
        {
            sClient->SocketClients->sendTextMessage(message);
            break;
        }
    }
}
void ChatiGateServer::commandProcess(QString message, QWebSocket *pSender){
    //    qDebug() << message;
    //    return;
    //    message = message.replace(" ","");
    QJsonDocument d = QJsonDocument::fromJson(message.toUtf8());
    QJsonObject command = d.object();
    QString getCommand =  QJsonValue(command["menuID"]).toString();

    //    qDebug() << "getCommand" << getCommand ;//<< message;
    if (getCommand == "") {
        qDebug()<< "commandProcess not found" << message;
        return;
    }
    if (getCommand == "register")
    {
        // qDebug() << "register_message" << message;
        int softPhoneID = QJsonValue(command["iGateID"]).toInt();
        SoftPhoneSocketClient *sClient = new SoftPhoneSocketClient;
        sClient->softPhoneID = softPhoneID;
        sClient->SocketClients = pSender;
        recSocketClient.append(sClient);
        // qDebug() <<  "append recSocketClient" << recSocketClient.length();
    }
    else if (getCommand == "applyRecSettings") {
        // qDebug() <<  "applyRecSettings:>>" << recSocketClient.length() << command;

        handleApplyRecSettings(command);
    }
    else if (getCommand == ("recLogging"))
    {
        // qDebug() <<  "append recSocketClient recLogging:" << recSocketClient.length() << message;

        int softPhoneID = QJsonValue(command["iGateID"]).toInt();
        int recorderID = QJsonValue(command["iGateID"]).toInt();
        QString recState = QJsonValue(command["state"]).toString();
        if (softPhoneID >4) softPhoneID = softPhoneID-4;
        emit recLogging(softPhoneID,recorderID,recState,message);
    }
    else if (getCommand == ("startSoftPhone"))
    {
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        if (softPhoneID > 0)
            emit newSoftphone(softPhoneID, pSender);

        bool found = false;
        Q_FOREACH (SoftPhoneSocketClient *sClient, softPhoneSocketClient)
        {
            if (sClient->softPhoneID == softPhoneID)
            {
                sClient->SocketClients = pSender;
                found = true;
            }
        }
        if (found == false)
        {
            SoftPhoneSocketClient *sClient = new SoftPhoneSocketClient;
            sClient->softPhoneID = softPhoneID;
            sClient->SocketClients = pSender;
            softPhoneSocketClient.append(sClient);
        }
    }
    else if (getCommand == ("trxConfig"))
    {
        int trxID = QJsonValue(command["trxID"]).toInt();
        QString CallName =  QJsonValue(command["CallName"]).toString();
        QString Address =  QJsonValue(command["Address"]).toString();
        QString sipPort =  QJsonValue(command["sipPort"]).toString();
        QString trxMode =  QJsonValue(command["trxMode"]).toString();
        QString active =  QJsonValue(command["enable"]).toString();
        QString CallNameRx =  QJsonValue(command["CallNameRx"]).toString();
        QString AddressRx =  QJsonValue(command["AddressRx"]).toString();
        QString sipPortRx =  QJsonValue(command["sipPortRx"]).toString();
        QString channelName =  QJsonValue(command["channelName"]).toString();

        // qDebug() << trxID << CallName << Address << sipPort << trxMode << active;
        emit updateTRxConfig(trxID, CallName, Address, sipPort, trxMode, active, CallNameRx,  AddressRx,  sipPortRx,  channelName);
    }

    else if (getCommand == ("newRadio")) {
        qDebug() << "getCommand newRadio" << message;
        QString radioName =  QJsonValue(command["radioName"]).toString();
        QString trxMode =  QJsonValue(command["trxMode"]).toString();
        QString uri =  QJsonValue(command["uri"]).toString();
        QString ipAddress =  QJsonValue(command["ipAddress"]).toString();
        int r2sPeriod = QJsonValue(command["r2sPeriod"]).toInt();
        int sipPort = QJsonValue(command["sipPort"]).toInt();
        QString frequenicy =  QJsonValue(command["frequenicy"]).toString();
        emit addNewRadio(radioName, trxMode, uri, ipAddress, r2sPeriod, sipPort, frequenicy);
    }

    else if (getCommand == ("updateRadio")) {
        int radioID = QJsonValue(command["radioID"]).toInt();
        QString radioName =  QJsonValue(command["radioName"]).toString();
        QString trxMode =  QJsonValue(command["trxMode"]).toString();
        QString uri =  QJsonValue(command["uri"]).toString();
        QString ipAddress =  QJsonValue(command["ipAddress"]).toString();
        int r2sPeriod = QJsonValue(command["r2sPeriod"]).toInt();
        int sipPort = QJsonValue(command["sipPort"]).toInt();
        QString frequenicy =  QJsonValue(command["frequenicy"]).toString();
        emit updateRadio(radioName, trxMode, uri, ipAddress, r2sPeriod, sipPort, frequenicy, radioID);
    }

    else if (getCommand == ("removeRadio")) {
        int radioID = QJsonValue(command["radioID"]).toInt();
        emit removeRadio(radioID);
    }

    else if (getCommand == ("addNewChannel"))
    {
        QString channelName =  QJsonValue(command["channelName"]).toString();
        QString mainRadioType =  QJsonValue(command["mainRadioType"]).toString();
        int mainRadio01ID = QJsonValue(command["mainRadio01ID"]).toInt();
        int mainRadio02ID = QJsonValue(command["mainRadio02ID"]).toInt();
        bool standbyEnable = QJsonValue(command["standbyEnable"]).toInt();
        QString standbyRadioType =  QJsonValue(command["standbyRadioType"]).toString();
        int standbyRadio01ID = QJsonValue(command["standbyRadio01ID"]).toInt();
        int standbyRadio02ID = QJsonValue(command["standbyRadio02ID"]).toInt();
        QString mainRadioName =  QJsonValue(command["mainRadioName"]).toString();
        QString standbyRadioName =  QJsonValue(command["standbyRadioName"]).toString();
        emit addNewChannel( channelName,  mainRadioType,  mainRadio01ID,  mainRadio02ID,  standbyEnable,  standbyRadioType,  standbyRadio01ID,  standbyRadio02ID,  mainRadioName,  standbyRadioName);
    }


    else if (getCommand == "updateChannel") {
        QString channelName =  QJsonValue(command["channelName"]).toString();
        QString mainRadioType =  QJsonValue(command["mainRadioType"]).toString();
        int mainRadio01ID = QJsonValue(command["mainRadio01ID"]).toInt();
        int mainRadio02ID = QJsonValue(command["mainRadio02ID"]).toInt();
        bool standbyEnable = QJsonValue(command["standbyEnable"]).toInt();
        QString standbyRadioType =  QJsonValue(command["standbyRadioType"]).toString();
        int standbyRadio01ID = QJsonValue(command["standbyRadio01ID"]).toInt();
        int standbyRadio02ID = QJsonValue(command["standbyRadio02ID"]).toInt();
        QString mainRadioName =  QJsonValue(command["mainRadioName"]).toString();
        QString standbyRadioName =  QJsonValue(command["standbyRadioName"]).toString();
        int channelID = QJsonValue(command["channelID"]).toInt();
        emit updateChannel( channelName,  mainRadioType,  mainRadio01ID,  mainRadio02ID,  standbyEnable,  standbyRadioType,  standbyRadio01ID,  standbyRadio02ID,  mainRadioName,  standbyRadioName, channelID);
    }

    else if (getCommand == ("removeChannel")) {
        int channelID = QJsonValue(command["channelID"]).toInt();
        emit removeChannel(channelID);
    }

    else if (getCommand == ("updateChannelTxRxEnable")) {
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        bool mainRadioTransmitterUsed = QJsonValue(command["mainRadioTransmitterUsed"]).toInt();
        bool mainRadioReceiverUsed = QJsonValue(command["mainRadioReceiverUsed"]).toInt();
        emit updateChannelTxRxEnable(mainRadioTransmitterUsed, mainRadioReceiverUsed, softPhoneID);
    }
    else if (getCommand == ("updateChannelID")) {
        int channelID = QJsonValue(command["channelID"]).toInt();
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit updateChannelID(channelID, softPhoneID);
    }
    else if (getCommand == ("updateControler")) {
        QString sipUser =  QJsonValue(command["sipUser"]).toString();
        //        int channelID = QJsonValue(command["channelID"]).toInt();
        int keepAlivePeroid = QJsonValue(command["keepAlivePeroid"]).toInt();
        int sipPort = QJsonValue(command["sipPort"]).toInt();
        int rtpStartPort = QJsonValue(command["rtpStartPort"]).toInt();
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        int defaultEthernet = QJsonValue(command["defaultEthernet"]).toInt();
        emit updateControler( sipUser,  keepAlivePeroid,  sipPort,  rtpStartPort, softPhoneID, defaultEthernet);
    }
    else if (getCommand == ("updateRelayOutputMode")) {
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        uint8_t RelayOutputMode = QJsonValue(command["RelayOutputMode"]).toInt();
        emit updateRelayOutputMode(RelayOutputMode,softPhoneID);
    }
    else if (getCommand == ("sipStarted"))
    {
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit sipStarted(softPhoneID);
    }
    else if (getCommand == ("updateCATISCONF"))
    {
        QString enableRecording = QJsonValue(command["enableRecording"]).toString();
        int warningAudioLevelTime =  QJsonValue(command["warningAudioLevelTime"]).toInt();
        int warningPercentFault =  QJsonValue(command["warningPercentFault"]).toInt();
        int warningPTTMinute =  QJsonValue(command["warningPTTMinute"]).toInt();
        int backupAudioMin =  QJsonValue(command["backupAudioMin"]).toInt();

        emit updateCATISCONF(enableRecording, warningAudioLevelTime, warningPercentFault, warningPTTMinute, backupAudioMin);
    }

    else if (getCommand == ("trxReconnect"))
    {
        int trxID = QJsonValue(command["trxID"]).toInt();
        emit reconnectTRx(trxID);
    }
    else if (getCommand == ("updatePortInterface"))
    {
        int value = QJsonValue(command["portInterface"]).toInt();
        emit updatePortInterface(value);
    }
    else if (getCommand == ("updatetxScheduler"))
    {
        int value = QJsonValue(command["pttScheduler"]).toInt();
        emit updatetxScheduler(value);
    }
    else if (getCommand == ("updateNTPServer"))
    {
        QString value = QJsonValue(command["ntpServer"]).toString();
        emit updateNTPServer(value);
    }
    else if (getCommand == ("updateTime"))
    {
        QString value = QJsonValue(command["dateTime"]).toString();
        emit setcurrentDatetime(value);
    }
    else if (getCommand == ("setLocation"))
    {
        QString value = QJsonValue(command["location"]).toString();
        emit setLocation(value);
    }

    else if (getCommand == ("updateswitchInviteMode"))
    {
        int value = QJsonValue(command["inviteModeIndex"]).toInt();
        emit updateswitchInviteMode(value);
    }

    else if (getCommand == ("updateSiteTone"))
    {
        float value = QJsonValue(command["sidetone"]).toDouble();
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit updateSiteTone(value,softPhoneID);

    }
    else if (getCommand == ("updateGroupMute"))
    {
        uint8_t value = QJsonValue(command["groupMute"]).toInt();
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit updateGroupMute(value,softPhoneID);

    }
    else if (getCommand == ("updatePttDelay"))
    {
        uint8_t value = QJsonValue(command["pttDelay"]).toInt();
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit updatePttDelay(value,softPhoneID);

    }
    else if (getCommand == ("onLocalSidetoneLoopbackChanged"))
    {
        bool value = QJsonValue(command["localSidetoneLoopbackOn"]).toDouble();
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit onLocalSidetoneLoopbackChanged(value,softPhoneID);

    }
    else if (getCommand == ("updateLocalSidetone"))
    {
        float value = QJsonValue(command["localSidetone"]).toDouble();
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit updateLocalSiteTone(value,softPhoneID);

    }
    else if (getCommand == ("updateFirmware"))
    {
        emit updateFirmware();
    }
    else if (getCommand == ("toggleGpioOut"))
    {
        int gpioNum = QJsonValue(command["gpioNum"]).toInt();
        int gpioVal = QJsonValue(command["gpioVal"]).toInt();
        emit toggleGpioOut(gpioNum, gpioVal);
    }


    else if (getCommand == ("updateLocalNetwork"))
    {
        uint8_t dhcpmethod =  QJsonValue(command["dhcpmethod"]).toString() == "on";
        QString ipaddress =  QJsonValue(command["ipaddress"]).toString();
        QString subnet =  QJsonValue(command["subnet"]).toString();
        QString gateway =  QJsonValue(command["gateway"]).toString();
        QString pridns =  QJsonValue(command["pridns"]).toString();
        QString secdns =  QJsonValue(command["secdns"]).toString();
        QString phyNetworkName = QJsonValue(command["phyNetworkName"]).toString();
        // qDebug() << dhcpmethod << ipaddress << subnet << gateway << pridns << secdns;
        emit updateNetwork(dhcpmethod, ipaddress, subnet, gateway, pridns, secdns, phyNetworkName);
        bool reboot = QJsonValue(command["reboot"]).toInt() == 1;
        if (reboot)
        {
            system("sync");
            emit rebootSystem();
        }
    }
    else if (getCommand == ("restartnetwork"))
    {
        emit restartnetwork();
    }
    else if (getCommand == ("rebootSystem"))
    {
        system("systemctl restart iGate4x.service"); //reboot
        emit rebootSystem();
    }
    else if (getCommand == ("updateTestmode"))
    {
        emit updateTestmode(QJsonValue(command["testModeEnable"]).toInt());
    }
    else if (getCommand == ("getMonitorPage"))
    {
        m_WebSocketClients << pSender;
        // emit getMonitorPage(pSender);
        Q_FOREACH (SoftPhoneSocketClient *pClient, softPhoneSocketClient)
        {
            pClient->SocketClients->sendTextMessage(message);
        }
    }
    else if (getCommand == ("iGateGroupEnableService"))
    {
        Q_FOREACH (QWebSocket *webClient, m_WebSocketClients)
        {
            webClient->sendTextMessage(message);
        }
    }
    else if (getCommand == ("getNetworkPage"))
    {
        m_WebSocketClients << pSender;
        emit getNetworkPage(pSender);
    }
    else if (getCommand == ("getSystemPage"))
    {
        m_WebSocketClients << pSender;
        emit getSystemPage(pSender);
    }
    else if (getCommand == ("getCtrlinfoPage"))
    {
        bool hasConnect = false;
        Q_FOREACH (QWebSocket *webClient, m_WebSocketClients)
        {
            if (webClient == pSender){
                hasConnect = true;
                break;
            }
        }
        if (!hasConnect)
            m_WebSocketClients << pSender;

        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit getCtrlInfoPage(softPhoneID);
    }
    else if (getCommand == ("pttGroupActive")) // only passthrough message to softPhoneID
    {
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        Q_FOREACH (SoftPhoneSocketClient *sClient, softPhoneSocketClient)
        {
            if (sClient->softPhoneID == softPhoneID)
            {
                sClient->SocketClients->sendTextMessage(message);
                break;
            }
        }
    }
    else if (getCommand == ("channelMessage"))
    {
        Q_FOREACH (QWebSocket *webClient, m_iGateGroupMngWebSocketClients)
        {

            if(QJsonValue(command["pttInput_count"]).toInt() <= 1){
                //                qDebug() << "channelMessage" << message;
                webClient->sendTextMessage(message);
            }
        }
        int pttInput_count = QJsonValue(command["pttInput_count"]).toInt();
        double freq = 0.0;
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        int trxMode = QJsonValue(command["trxMode"]).toInt();
        bool mainTxPTTOn = QJsonValue(command["mainTxPTTOn"]).toInt();
        bool mainRxSQLOn = QJsonValue(command["mainRxSQLOn"]).toInt();
        bool standbyTxPTTOn = QJsonValue(command["standbyTxPTTOn"]).toInt();
        bool standbySQLOn = QJsonValue(command["standbyRxSQLOn"]).toInt();
        int numConn = QJsonValue(command["numConn"]).toInt();
        int numTxRx = QJsonValue(command["numTxRx"]).toInt();
        QString trxType = QJsonValue(command["mainRadioTRxType"]).toString();
        QString mainRadio01URI = QJsonValue(command["mainRadio01URI"]).toString();
        QString mainRadio02URI = QJsonValue(command["mainRadio02URI"]).toString();
        QString standbyRadio01URI = QJsonValue(command["standbyRadio01URI"]).toString();
        QString standbyRadio02URI = QJsonValue(command["standbyRadio02URI"]).toString();
        QString mainRadio01IPAddr = QJsonValue(command["mainRadio01IPAddr"]).toString();
        QString mainRadio02IPAddr = QJsonValue(command["mainRadio02IPAddr"]).toString();
        QString standbyRadio01IPAddr = QJsonValue(command["standbyRadio01IPAddr"]).toString();
        QString standbyRadio02IPAddr = QJsonValue(command["standbyRadio02IPAddr"]).toString();


        bool main01_is_mainTx = QJsonValue(command["main01_is_mainTx"]).toInt();
        bool main01_is_mainRx = QJsonValue(command["main01_is_mainRx"]).toInt();
        bool main02_is_mainRx = QJsonValue(command["main02_is_mainRx"]).toInt();
        bool standby01_is_mainTx = QJsonValue(command["standby01_is_mainTx"]).toInt();
        bool standby01_is_mainRx = QJsonValue(command["standby01_is_mainRx"]).toInt();
        bool standby02_is_mainRx = QJsonValue(command["standby02_is_mainRx"]).toInt();

        double mainTxFrequency = (QJsonValue(command["mainTxFrequency"]).toString().toInt())/1e6;
        double mainRxFrequency = (QJsonValue(command["mainRxFrequency"]).toString().toInt())/1e6;

        bool pttOn = ((main01_is_mainTx & mainTxPTTOn) || (standby01_is_mainTx & standbyTxPTTOn));
        bool sqlOn = ((main01_is_mainRx & mainRxSQLOn) || (standby01_is_mainRx & standbySQLOn) ||
                      (main02_is_mainRx & mainRxSQLOn) || (standby02_is_mainRx & standbySQLOn));

        freq = mainTxFrequency != 0 ? mainTxFrequency : mainRxFrequency;
        // qDebug() << "mainTxFrequency" << mainTxFrequency << mainRxFrequency << message;
        emit appendChannelData(softPhoneID, trxMode, pttOn, sqlOn, numConn, numTxRx, trxType, mainRadio01IPAddr, mainRadio02IPAddr, standbyRadio01IPAddr, standbyRadio02IPAddr, freq, pttInput_count);
        Q_FOREACH (QWebSocket *webClient, m_WebSocketClients)
        {
            webClient->sendTextMessage(message);
        }
    }
    else if (getCommand == ("toggleRxEnable"))
    {
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit toggleRxEnable(softPhoneID);
    }
    else if (getCommand == ("toggleTxEnable"))
    {
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit toggleTxEnable(softPhoneID);
    }
    else if (getCommand == ("restartSoftphone"))
    {
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit restartSoftphone(softPhoneID);
    }
    else if (getCommand == ("connStatus"))
    {
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        QString lastPttOn = QJsonValue(command["pttURI"]).toString();
        QString trxStatus = QJsonValue(command["TxRx"]).toString();
        QString pttURI = QJsonValue(command["pttURI"]).toString();
        bool mainTxPTTOn = (trxStatus == PTTON) ||  (trxStatus == PTTRX);
        bool mainRxSQLOn = (trxStatus == RXON)  ||  (trxStatus == PTTRX);
        int numConn = QJsonValue(command["connNum"]).toInt();
        double channelFreq = QJsonValue(command["channelFreq"]).toDouble();
        emit appendClientConnStatus(softPhoneID, lastPttOn, trxStatus, pttURI, mainTxPTTOn, mainRxSQLOn, numConn);
        emit onSendSquelchStatus(softPhoneID, mainTxPTTOn, mainRxSQLOn, numConn>0, channelFreq);
    }
    else if (getCommand == ("ConnDuration"))
    {
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        QString uri = QJsonValue(command["uri"]).toString();
        QString duration = QJsonValue(command["duration"]).toString();
        bool pttOn = QJsonValue(command["pttOn"]).toInt();
        bool sqlOn = QJsonValue(command["sqlOn"]).toInt();
        bool callState = QJsonValue(command["callState"]).toInt();
        emit appendClientCallInStatus(softPhoneID, uri,duration,pttOn, sqlOn, callState);
    }
    else if (getCommand == ("igateNumberSelect"))
    {
        int softPhoneID = QJsonValue(command["iGateNum"]).toInt();
        m_WebSocketClients << pSender;
        emit getHomePage(softPhoneID, pSender);
    }
    else if (getCommand == ("getVuMeter"))
    {
        m_WebSocketVUClients << pSender;
        emit getVuMeter(pSender);
    }
    else if (getCommand == ("getServerHomePage"))
    {
        int softPhoneID = QJsonValue(command["iGateNum"]).toInt();
        if (softPhoneID == 1)
            m_WebSocketClients << pSender;
        emit getServerHomePage(softPhoneID, pSender);
    }
    else if (getCommand == ("iGateGroupReg"))
    {
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        m_iGateGroupMngWebSocketClients << pSender;
        emit getIGateGroupConfig(softPhoneID, pSender);
        bool found = false;
        Q_FOREACH (SoftPhoneSocketClient *sClient, igateGroupMngSocketClient)
        {
            if (sClient->softPhoneID == softPhoneID)
            {
                sClient->SocketClients = pSender;
                found = true;
            }
        }
        if (found == false)
        {
            SoftPhoneSocketClient *sClient = new SoftPhoneSocketClient;
            sClient->softPhoneID = softPhoneID;
            sClient->SocketClients = pSender;
            igateGroupMngSocketClient.append(sClient);
        }
    }
    else if (getCommand == ("newIGateGroup"))
    {
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        int iGateID = QJsonValue(command["iGateID"]).toInt();
        QString ipaddress  = QJsonValue(command["ipaddress"]).toString();
        QString uriName = QString("iGateID%1").arg(iGateID);
        emit addIGateGroupMng(softPhoneID,iGateID,ipaddress,uriName);
    }
    else if (getCommand == ("syncIGateGroup")) {
        int softPhoneID = QJsonValue(command["toSoftPhoneID"]).toInt();
        int iGateID = QJsonValue(command["iGateID"]).toInt();
        QString toIPAddress  = QJsonValue(command["toIPAddress"]).toString();
        QString ipaddress  = QJsonValue(command["ipaddress"]).toString();
        QString uriName = QString("iGateID%1").arg(iGateID);
        emit syncIGateGroup(toIPAddress, softPhoneID,iGateID,ipaddress,uriName);
    }
    else if (getCommand == ("deleteIGate")) {
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        int iGateID = QJsonValue(command["iGateID"]).toInt();
        QString ipaddress  = QJsonValue(command["ipaddress"]).toString();
        int id = QJsonValue(command["id"]).toInt();
        emit deleteIGate(softPhoneID, id,iGateID,ipaddress);
    }
    else if (getCommand == ("clearIGateGroup")) {
        qDebug() << "clearIGateGroup message" << message;
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit clearIGateGroup(softPhoneID);
    }
    else if (getCommand == ("updateTalkGroupEnableService")) {
        qDebug() << "updateTalkGroupEnableService message" << message;
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        sendMessageToSoftPhone(message,softPhoneID);
    }
    else if (getCommand == ("deleteIGateGroup")) {
        int softPhoneID = QJsonValue(command["toSoftPhoneID"]).toInt();
        QString toIPAddress  = QJsonValue(command["toIPAddress"]).toString();
        emit deleteIGateGroup(toIPAddress, softPhoneID);
    }
    else if (getCommand == ("syncDeleteIGateAnotherGroup")) {
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        int iGateID = QJsonValue(command["iGateID"]).toInt();
        QString ipaddress0  = QJsonValue(command["ipaddress0"]).toString();
        QString ipaddress1  = QJsonValue(command["ipaddress1"]).toString();
        emit syncDeleteIGateAnotherGroup(iGateID,softPhoneID,ipaddress0,ipaddress1);
    }
    else if (getCommand == ("updateDeviceName"))
    {
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        QString deviceName = QJsonValue(command["deviceName"]).toString();

        emit updateDeviceName(softPhoneID, deviceName);
        broadcastMessage(message);
    }
    else if (getCommand == ("updateHostCfg"))
    {
        uint8_t softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        int keepAlivePeroid = QJsonValue(command["keepAlivePeroid"]).toInt();
        int sipPort = QJsonValue(command["sipPort"]).toInt();
        QString sipUser =  QJsonValue(command["name"]).toString();
        QString defaultEthernet = QJsonValue(command["defaultEthernet"]).toString();
        // qDebug() << "updateHostCfg" << sipUser << sipPort << keepAlivePeroid << defaultEthernet;

        emit updateHostConfig(sipUser, sipPort, keepAlivePeroid, defaultEthernet, softPhoneID);

        broadcastMessage(message);
    }
    else if (getCommand == ("updateInputgain"))
    {
        uint8_t value = QJsonValue(command["inputGainIndex"]).toInt();
        uint8_t softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit updateInputGain(value,softPhoneID);
        broadcastMessage(message);
    }
    else if (getCommand == ("updateRxBestSignalEnable"))
    {
        uint8_t value = QJsonValue(command["value"]).toInt();
        uint8_t softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit updateRxBestSignalEnable(value,softPhoneID);
        broadcastMessage(message);
    }
    else if (getCommand == ("updateOutputgain"))
    {
        uint8_t value = QJsonValue(command["outputGainIndex"]).toInt();
        uint8_t softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit updateOutputGain(value,softPhoneID);
        broadcastMessage(message);
    }
    else if (getCommand == ("updateDSPOutputGain"))
    {
        int value = QJsonValue(command["outputGainIndex"]).toInt();
        uint8_t softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit updateDSPOutputGain(value,softPhoneID);
        broadcastMessage(message);
    }

    else if (getCommand == ("updateDSPRecInputGain"))
    {
        uint8_t value = QJsonValue(command["inputGainIndex"]).toInt();
        uint8_t softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit updateDSPRecInputGain(value,softPhoneID);
        broadcastMessage(message);
    }
    else if (getCommand == ("updateRecAddress"))
    {
        QString value = QJsonValue(command["value"]).toString();
        int index = QJsonValue(command["index"]).toInt();
        uint8_t softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit updateRecAddress(value,index,softPhoneID);
        broadcastMessage(message);
    }
    else if (getCommand == ("updateDSPRecOutputGain"))
    {
        uint8_t value = QJsonValue(command["outputGainIndex"]).toInt();
        uint8_t softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit updateDSPRecOutputGain(value,softPhoneID);
        broadcastMessage(message);
    }
    else if (getCommand == ("disconnect"))
    {
        qDebug() << "message disconnect" << message;
        broadcastMessage(message);
    }
    else if (getCommand == ("updateURILits"))
    {
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        QStringList urlList = QJsonValue(command["urlList"]).toString().split(",");
        emit updateURILits(urlList, softPhoneID);

        int numconn = QJsonValue(command["numconn"]).toInt();
        emit updateNumconn(numconn,softPhoneID);

        broadcastMessage(message);
    }
    else if (getCommand == ("updateSQLDefeat"))
    {
        uint8_t softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit updateSQLDefeat(QJsonValue(command["sqlDefeat"]).toString().contains("true"),softPhoneID);
        broadcastMessage(message);
    }
    else if (getCommand == ("updateSQLActiveHigh"))
    {
        uint8_t softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit updateSQLActiveHigh(QJsonValue(command["sqlActive"]).toString().contains("true"),softPhoneID);
        broadcastMessage(message);
    }
    else if (getCommand == ("RadioActive"))
    {
        //        qDebug() << "RadioActive" << message;
        uint8_t softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        bool radioAutoInactive = QJsonValue(command["radioAutoInactive"]).toString().contains("true") || (QJsonValue(command["radioAutoInactive"]).toInt() == 1);
        //        bool txMainRadioUsed = QJsonValue(command["mainRadioTransmitterUsed"]).toString().contains("true") || (QJsonValue(command["mainRadioTransmitterUsed"]).toInt() == 1);
        //        bool rxMainRadioUsed = QJsonValue(command["mainRadioReceiverUsed"]).toString().contains("true") || (QJsonValue(command["mainRadioReceiverUsed"]).toInt() == 1);
        int radioMainStandby = QJsonValue(command["radioMainStandby"]).toInt();
        emit updateRadioActive(radioAutoInactive,1,1, radioMainStandby, softPhoneID);
        sendToWebMessageClient(message);
    }
    else if (getCommand == ("updateRadioAutoInactive"))
    {
        uint8_t softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit updateRadioAutoInactive(QJsonValue(command["radioAutoInactive"]).toString().contains("true") || (QJsonValue(command["radioAutoInactive"]).toInt() == 1),softPhoneID);
        broadcastMessage(message);
    }
    else if (getCommand == ("updateRadioMainStandby"))
    {
        uint8_t softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit updateRadioMainStandby(QJsonValue(command["radioMainStandby"]).toInt(),softPhoneID);
        broadcastMessage(message);
    }
    else if (getCommand == ("updateMainRadioReceiverUsed"))
    {
        uint8_t softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit updateMainRadioReceiverUsed(QJsonValue(command["mainRadioReceiverUsed"]).toString().contains("true") || (QJsonValue(command["mainRadioReceiverUsed"]).toInt() == 1),softPhoneID);
        broadcastMessage(message);
    }
    else if (getCommand == ("updateMainRadioTransmitterUsed"))
    {
        uint8_t softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit updateMainRadioTransmitterUsed(QJsonValue(command["mainRadioTransmitterUsed"]).toString().contains("true") || (QJsonValue(command["mainRadioTransmitterUsed"]).toInt() == 1),softPhoneID);
        broadcastMessage(message);
    }
    else if (getCommand == ("MCP3208_0"))
    {
        int adcID = QJsonValue(command["channel"]).toInt();
        double value = QJsonValue(command["value"]).toDouble();
        emit adcValue(adcID, value, message);
    }
    else if (getCommand == "systembackup"){
        emit systembackup();
    }
    else if (getCommand == "systemrestore")
    {
        emit systemrestore();
    }
    else if (getCommand == ("setRadioController"))
    {
        uint8_t softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit setRadioController(QJsonValue(command["RadioController"]).toInt(),softPhoneID);
    }
    //TONE getCommand
    else if (getCommand == ("updateInputTone"))
    {
        bool ToneState = QJsonValue(command["outputToneState"]).toString().contains("true") || (QJsonValue(command["outputToneState"]).toBool() == 1);
        float ToneFrequency = QJsonValue(command["outputToneFrequency"]).toDouble();
        int TonePhase = QJsonValue(command["outputTonePhase"]).toInt();
        float ToneLevel = QJsonValue(command["outputToneLevel"]).toDouble();
        uint8_t softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        emit updateInputTone(ToneState,ToneFrequency,TonePhase,ToneLevel,softPhoneID);
        sendToWebMessageClient(message);
    }
    else if (getCommand == "updateFIRfilter"){
        bool FIRfilterInput = QJsonValue(command["firActiveStateInput"]).toString().contains("true") || (QJsonValue(command["firActiveStateInput"]).toBool() == 1);
        bool FIRfilterOutput = QJsonValue(command["firActiveStateOutput"]).toString().contains("true") || (QJsonValue(command["firActiveStateOutput"]).toBool() == 1);
        //        uint8_t softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        //         qDebug() << "FIRinput : " << FIRfilterInput << "FIRoutput : " << FIRfilterOutput;
        emit updateFIRfilter(FIRfilterInput,FIRfilterOutput);
        //        qDebug()<<"qqooo"<< QJsonValue(command["firActiveState"]).toString().contains("true");
        sendToWebMessageClient(message);
    }
    else if (getCommand == ("updateTimeout"))
    {
        emit updateNetworkTimeout(QJsonValue(command["NetworkTimeout"]).toInt());
        sendToWebMessageClient(message);
    }
    else if (getCommand == ("updateToneServer"))
    {
        bool ToneServerState = QJsonValue (command ["ToneServerState"]).toString().contains("true") || (QJsonValue(command["ToneServerState"]).toBool() == 1);
        bool pttState = QJsonValue (command ["pttState"]).toString().contains("true") || (QJsonValue(command["pttState"]).toBool() == 1);
        int timeoutptt = QJsonValue(command["timeoutptt"]).toInt();
        float ToneServerFrequency = QJsonValue(command["ToneServerFrequency"]).toDouble();
        int ToneServerPhase = QJsonValue(command["ToneServerPhase"]).toInt();
        float ToneServerLevel = QJsonValue(command["ToneServerLevel"]).toDouble();
        uint8_t softPhoneID = QJsonValue(command["softPhoneID"]).toInt();

        emit updateServerTone(ToneServerState,pttState,timeoutptt,ToneServerFrequency,ToneServerPhase,ToneServerLevel,softPhoneID);
        //    qDebug()<<"updateToneServer"<<"ToneServerState"<<ToneServerState<<"pttState"<<pttState<<"timeoutptt"<<timeoutptt
        //           <<"ToneServerFrequency"<<ToneServerFrequency<<"ToneServerPhase"<<ToneServerPhase
        //          <<"ToneServerLevel"<<ToneServerLevel<<"softPhoneID"<<softPhoneID;

    }
    else if (getCommand == ("getRedioTestHomePage"))
    {
        int softPhoneID = QJsonValue(command["iGateNum"]).toInt();
        if (softPhoneID == 1)
            m_WebSocketClients << pSender;
        emit getRedioTestHomePage(softPhoneID, pSender);
    }
    else if (getCommand == ("m_WebSocketRecClients"))
    {
        m_WebSocketRecClients << pSender;
    }
    else if (getCommand == ("EventDataLogger"))
    {
        QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
        if (!doc.isObject()) {
            qDebug() << "Invalid JSON format!";
            return;
        }

        QJsonObject messageObj = doc.object();

        // Now call the InsertEventDataLogger function with the parsed JSON object
        emit EventDataLogger(messageObj);
    }
    else if ((getCommand == ("MCHSEL")) || (getCommand == ("MCHRFPWR")) || (getCommand == ("SETMSQLLV")) || (getCommand == "CTRLRSSI") || (getCommand == "CTRLBATV") || (getCommand == "serialConnect") || (getCommand =="CTRLTX") || (getCommand == "CTRLEPTT") || (getCommand =="CTRLSQL"))
    {
        Q_FOREACH (QWebSocket *pClient, m_WebSocketClients)
        {
            pClient->sendTextMessage(message);
        }
    }
    else if(getCommand == "CurrentRadioInfo")
    {
        int softPhoneID = QJsonValue(command["softPhoneID"]).toInt();
        qDebug() << "sendMessageToSoftPhone" << message << softPhoneID;
        sendMessageToSoftPhone(message,softPhoneID);
    }
    else
    {
        // qDebug() << "getCommand" << getCommand ;//<< message;
    }
}

void ChatiGateServer::processMessage(QString message)
{
    // qDebug() << message;
    QWebSocket *pSender = qobject_cast<QWebSocket *>(sender());
    commandProcess(message, pSender);
    emit onNewMessage(message);
}

void ChatiGateServer::socketDisconnected()
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient)
    {
        emit removeSoftphone(pClient);
        m_clients.removeAll(pClient);
        m_WebSocketClients.removeAll(pClient);
        m_iGateGroupMngWebSocketClients.removeAll(pClient);
        m_WebSocketVUClients.removeAll(pClient);
        m_WebSocketRecClients.removeAll(pClient);

        Q_FOREACH (SoftPhoneSocketClient *sClient, softPhoneSocketClient)
        {
            if (sClient->SocketClients == pClient)
            {
                softPhoneSocketClient.removeAll(sClient);
                break;
            }
        }
        Q_FOREACH (SoftPhoneSocketClient *sClient, igateGroupMngSocketClient)
        {
            if (sClient->SocketClients == pClient)
            {
                igateGroupMngSocketClient.removeAll(sClient);
                break;
            }
        }
        Q_FOREACH (SoftPhoneSocketClient *sClient, recSocketClient)
        {
            if (sClient->SocketClients == pClient)
            {
                recSocketClient.removeAll(sClient);
                break;
            }
        }

        pClient->deleteLater();
        qDebug() << pClient->localAddress().toString() << "has disconect";
    }
    clientNum = m_clients.length();
    if (clientNum <= 0)
    {
        onNumClientChanged(clientNum);
    }
}
