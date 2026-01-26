#include "SocketClient.h"

QT_USE_NAMESPACE

//! [constructor]
SocketClient::SocketClient(QObject *parent) :
    QObject(parent)
{
    connect(&m_webSocket, &QWebSocket::connected, this, &SocketClient::onConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &SocketClient::onDisconnected);
    connect(&m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
    [=](QAbstractSocket::SocketError error){
        if (isConnected){
            emit closed(m_socketID, m_ipaddress);
            isConnected = false;
        }
        qDebug() << "Connecting Error: " << error;
    });
    connect(&m_webSocket, &QWebSocket::textMessageReceived,
            this, &SocketClient::onTextMessageReceived);
}
//! [constructor]

//! [createConnection]
void SocketClient::createConnection(int softphoneID, int channelIdInRole, QString ipaddress, quint16 port)
{
    QString url = QString("ws://%1:%2").arg(ipaddress).arg(port);
    QUrl iGateSocketServerUrl(url);
    if (m_debug)
        qDebug() << "WebSocket server:" << url;
    m_url = iGateSocketServerUrl;
    m_ipaddress = ipaddress;
    softPhoneID = softphoneID;
    m_socketID = channelIdInRole;
    m_webSocket.open(QUrl(iGateSocketServerUrl));
}
//! [createConnection]

//! [onConnected]
void SocketClient::onConnected()
{
    QString message = QString("{\"menuID\":\"QmlRadioScanner\"}");

    isConnected = true;
    if (m_debug)
        qDebug() << "WebSocket connected";
    m_webSocket.sendTextMessage(message);
    m_webSocket.sendTextMessage("Hello, world!");
}
//! [onConnected]

//! [onTextMessageReceived]
void SocketClient::onTextMessageReceived(QString message)
{
    QJsonDocument d = QJsonDocument::fromJson(message.toUtf8());
    QJsonObject command = d.object();
    emit TextMessageReceived(message, m_socketID, m_ipaddress);
    emit newCommandProcess(command,&m_webSocket,message);
}

//! [onTextMessageReceived]

void SocketClient::onDisconnected()
{
    if (isConnected == true)
        qDebug() << m_ipaddress << "WebSocket disconnected";
    isConnected = false;    
    emit closed(m_socketID, m_ipaddress);
}

void SocketClient::onError(QAbstractSocket::SocketError error)
{
    qDebug() << "Connecting Error: " << error;
}
