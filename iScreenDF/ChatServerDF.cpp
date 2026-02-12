#include "ChatServerDF.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

QT_USE_NAMESPACE

ChatServerDF::ChatServerDF(const quint16 &port, QObject *parent) :
    QObject(parent),
    m_pWebSocketServer(Q_NULLPTR),
    m_clients(),
    clientNum(0)  // initialize clientNum
{
    m_pWebSocketServer = new QWebSocketServer(QStringLiteral("Chat Server"),
                                              QWebSocketServer::NonSecureMode,
                                              this);
    if (m_pWebSocketServer->listen(QHostAddress::Any, port))
    {
        qDebug() << "Chat Server listening on port" << port;
        connect(m_pWebSocketServer, &QWebSocketServer::newConnection,
                this, &ChatServerDF::onNewConnection);
    }
}

ChatServerDF::~ChatServerDF()
{
    m_pWebSocketServer->close();
    qDeleteAll(m_clients.begin(), m_clients.end());
}

void ChatServerDF::onNewConnection()
{
    QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();
    connect(pSocket, &QWebSocket::textMessageReceived,
            this, &ChatServerDF::processMessage);
    connect(pSocket, &QWebSocket::disconnected,
            this, &ChatServerDF::socketDisconnected);

    m_clients << pSocket;

    qDebug() << "On New Connection from address: " << pSocket->peerAddress().toString();

    clientNum = m_clients.length();
    emit onNumClientChanged(clientNum);
}

void ChatServerDF::socketDisconnected()
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient)
    {
        m_clients.removeAll(pClient);
        m_snmpSocketClients.removeAll(pClient);
        pClient->deleteLater();

        qDebug() << pClient->peerAddress().toString() << "has disconnected";
    }

    clientNum = m_clients.length();
    emit onNumClientChanged(clientNum);
}
void ChatServerDF::stopServer()
{
    qInfo() << "[ChatServerDF] stopServer() called";

    for (QWebSocket *c : qAsConst(m_clients)) {
        if (!c)
            continue;

        c->disconnect(this);

        c->close();

        c->deleteLater();
    }

    m_clients.clear();
    m_snmpSocketClients.clear();

    clientNum = 0;
    emit onNumClientChanged(clientNum);

    if (m_pWebSocketServer) {
        m_pWebSocketServer->disconnect(this);

        if (m_pWebSocketServer->isListening()) {
            m_pWebSocketServer->close();
        }

        qInfo() << "[ChatServerDF] WebSocket server closed";
    }
}

void ChatServerDF::processMessage(const QString &message)
{
    QWebSocket *pSender = qobject_cast<QWebSocket *>(sender());
    commandProcess(message, pSender);
}

void ChatServerDF::commandProcess(const QString &message, QWebSocket *pSender)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject())
    {
        qWarning() << "Invalid JSON message received:" << message;
        return;
    }

    QJsonObject command = doc.object();
    QString getCommand = command.value("menuID").toString();
    QString objectName = command.value("objectName").toString();

    emit newCommandProcess(command, pSender, message);
}

void ChatServerDF::serverSendMessage(const QString &message)
{
    for (QWebSocket *pClient : qAsConst(m_clients))
    {
        pClient->sendTextMessage(message);
    }
}

void ChatServerDF::broadcastMessage(const QString &message)
{
    for (QWebSocket *pClient : qAsConst(m_clients))
    {
        pClient->sendTextMessage(message);
    }
}
