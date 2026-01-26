#include "ChatClientiGate.h"

ChatClientiGate::ChatClientiGate(quint16 port, QString ipaddress, QString initMessage, int index, bool autoConnect, QObject *parent) :
    QObject(parent)
{
    url = QString("ws://%1:%2").arg(ipaddress).arg(port);
    autoConnectTimer = new QTimer(this);
    socketIndex = index;
    autoConnectEnable = autoConnect;
    m_pWebSocketClient = new QWebSocket;
    if(autoConnectEnable == false)
        m_pWebSocketClient->open(QUrl(url));
    init_message = initMessage;

    connect(m_pWebSocketClient,SIGNAL(connected()), this, SLOT(onConnected()));


    connect(autoConnectTimer,SIGNAL(timeout()),this,SLOT(autoConnectServer()));

    //    connect(m_pWebSocketClient,SIGNAL(textMessageReceived(QString)), this, SLOT(onTextMessageReceived(QString)));
    if (autoConnectEnable){
        qDebug() << "ChatClient::autoConnectEnable m_pWebSocketClient->open" << autoConnectEnable << url;
        m_pWebSocketClient->open(QUrl(url));
        autoConnectTimer->start(3000);
    }
}
void ChatClientiGate::autoConnectServer(){
    if (m_pWebSocketClient->state() != QAbstractSocket::ConnectedState)
        m_pWebSocketClient->open(QUrl(url));
    else
    {
        //        qDebug() << "m_pWebSocketClient->state() " << m_pWebSocketClient->state();
    }
}
void ChatClientiGate::sendTextMessage(QString message)
{
    m_pWebSocketClient->sendTextMessage(message);
}

void ChatClientiGate::broadcastMessageNodeState(uint8_t nodeID, QString conn, QString duration, QString trxStatus, QString radioStatus, QString vswr, QString durationRx, QString connRx, QString radioStatusRx,int softPhoneID)
{
    QString message = QString("{\"menuID\":\"connState\", \"nodeID\":%1, \"connStatus\":\"%2\", \"connDuration\":\"%3\", \"trxStatus\":\"%4\", \"radioStatus\":\"%5\", \"vswr\":"
                              "\"%6\", \"connDurationRx\":\"%7\", \"connStatusRx\":\"%8\", \"radioStatusRx\":\"%9\", \"softPhoneID\":%10}")
                          .arg(nodeID).arg(conn).arg(duration).arg(trxStatus).arg(radioStatus).arg(vswr).arg(durationRx).arg(connRx).arg(radioStatusRx).arg(softPhoneID);
    m_pWebSocketClient->sendTextMessage(message);
}
void ChatClientiGate::broadcastSystemMessage(QString nodeSelected, int softPhoneID)
{
    QString message = QString("{\"menuID\":\"nodeSelected\", \"softPhoneID\":%1, \"nodeSelected\":\"%3\"}")
    .arg(softPhoneID).arg(nodeSelected);
    m_pWebSocketClient->sendTextMessage(message);
}
void ChatClientiGate::onConnected()
{
    qDebug() << "WebSocket connected";
    if (init_message != "")
        m_pWebSocketClient->sendTextMessage(init_message);

    emit connected(socketIndex);

    connect(m_pWebSocketClient, SIGNAL(textMessageReceived(QString)),
            this, SLOT(onTextMessageReceived(QString)));
    connect(m_pWebSocketClient, SIGNAL(disconnected()),
            this, SLOT(onDisconnected()));
    if (autoConnectEnable == false){
        m_pWebSocketClient->close();
        m_pWebSocketClient->deleteLater();
    }

    else if(autoConnectEnable)
        autoConnectTimer->start(10000);
}
void ChatClientiGate::onDisconnected()
{
    qDebug() << "WebSocket disconnected";
    //    emit connected(socketIndex);
    if (autoConnectEnable)
        autoConnectTimer->start(3000);
}
void ChatClientiGate::onTextMessageReceived(QString message)
{
    emit textMessageReceived(message);
}
void ChatClientiGate::setDevice(QString _deviceName, QString _deviceAddress)
{
    deviceName = _deviceName;
    deviceAddress = _deviceAddress;
}
void ChatClientiGate::createDataLog(int state, QString dataLog)
{
    QJsonObject jsonData;
    QString strState = "";
    if (m_pWebSocketClient == nullptr) return;
    if (m_pWebSocketClient->state() == QAbstractSocket::ConnectedState)
    {
        strState = (state == 0) ? "Error" : (state == 1) ? "Info" : "";
        jsonData["menuID"] = "logInsert";
        jsonData["deviceName"] = deviceName;
        jsonData["deviceAddress"] = deviceAddress;
        jsonData["dataLog"] = dataLog;
        jsonData["state"] = strState;

        QJsonDocument jsonDoc(jsonData);
        QByteArray jsonDataBytes = jsonDoc.toJson();

        m_pWebSocketClient->sendBinaryMessage(jsonDataBytes);
    }
}
