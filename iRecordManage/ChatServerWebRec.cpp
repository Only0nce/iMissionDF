#include "ChatServerWebRec.h"

ChatServerWebRec::ChatServerWebRec(quint16 port, QObject *parent)
    : QObject(parent)
    , m_server(new QWebSocketServer(QStringLiteral("iRecordManageWebServer"),
                                    QWebSocketServer::NonSecureMode,
                                    this))
{
    if (!m_server->listen(QHostAddress::Any, port)) {
        qWarning() << "[ChatServerWebRec] Failed to listen on port" << port
                   << "error =" << m_server->errorString();
        return;
    }

    qInfo() << "[ChatServerWebRec] WebSocket server listening on port" << port;

    connect(m_server, &QWebSocketServer::newConnection,
            this, &ChatServerWebRec::onNewConnection);
}

ChatServerWebRec::~ChatServerWebRec()
{
    qInfo() << "[ChatServerWebRec] shutting down, closing" << m_clients.size() << "clients";

    // Detach lists first so close()/disconnected cannot mutate a container
    // currently being iterated during teardown.
    const auto clientsCopy = m_clients;
    m_clients.clear();
    m_WebSocketClients.clear();
    for (QWebSocket *socket : clientsCopy) {
        if (!socket)
            continue;
        QObject::disconnect(socket, nullptr, this, nullptr);
        socket->close();
        socket->deleteLater();
    }
    qDeleteAll(softPhoneSocketClient);
    qDeleteAll(recSocketClient);
    softPhoneSocketClient.clear();
    recSocketClient.clear();

    if (m_server) {
        m_server->close();
    }
}

void ChatServerWebRec::onNewConnection()
{
    QWebSocket *socket = m_server->nextPendingConnection();
    if (!socket)
        return;

    qInfo() << "[ChatServerWebRec] New client from"
            << socket->peerAddress().toString()
            << ":" << socket->peerPort();

    m_clients << socket;

    connect(socket, &QWebSocket::textMessageReceived,
            this,   &ChatServerWebRec::onTextMessageReceived);
    connect(socket, &QWebSocket::disconnected,
            this,   &ChatServerWebRec::onSocketDisconnected);

    // ===== ถูกต้องที่สุด: เช็ค socket จำนวนจริง =====
    if (m_clients.size() == 1) {
        enableDataLogger = true;
        qInfo() << "[ChatServerWebRec] enableDataLogger = TRUE (first client connected)";
    }
    // ================================================

    emit clientConnected(socket);
    emit clientCountChanged(m_clients.size());
}


void ChatServerWebRec::onTextMessageReceived(const QString &message)
{
    qDebug() << "[ChatServerWebRec] text message from web:"
             << message.left(200); // กัน log ยาวเกิน
    emit cppCommandToWeb(message);
    QWebSocket *pSender = qobject_cast<QWebSocket *>(sender());

    commandProcess(message, pSender);

    //    emit messageFromWeb(message, senderSocket);
}

void ChatServerWebRec::commandProcess(QString message, QWebSocket *pSender){
    qDebug() << "commandProcess:" << message;
    //    return;
    //    message = message.replace(" ","");
    QJsonDocument d = QJsonDocument::fromJson(message.toUtf8());
    QJsonObject command = d.object();
    QString getCommand =  QJsonValue(command["menuID"]).toString();

    //    qDebug() << "getCommand" << getCommand ;//<< message;
    if (getCommand == "") {
        qDebug()<< "commandProcess not found" << message;
        return;
    }else if (getCommand == ("getSystemPageWeb"))
    {
        qDebug() << "getSystemPageWeb:" << message;

        if (pSender && !m_WebSocketClients.contains(pSender))
            m_WebSocketClients << pSender;
        emit getSystemPage(pSender);
        emit getVuMeter(pSender);

    }

    else
    {
        // qDebug() << "getCommand" << getCommand ;//<< message;
    }
}

void ChatServerWebRec::onSocketDisconnected()
{
    QWebSocket *socket = qobject_cast<QWebSocket*>(sender());
    if (!socket)
        return;

    m_clients.removeAll(socket);
    m_WebSocketClients.removeAll(socket);

    // The socket can still be referenced by auxiliary wrappers in legacy
    // paths. Remove any wrapper that points at this connection before the
    // QObject is deleted.
    for (int i = recSocketClient.size() - 1; i >= 0; --i) {
        SoftPhoneSocketClient *client = recSocketClient.at(i);
        if (!client || client->SocketClients == socket) {
            delete client;
            recSocketClient.removeAt(i);
        }
    }
    for (int i = softPhoneSocketClient.size() - 1; i >= 0; --i) {
        SoftPhoneSocketClient *client = softPhoneSocketClient.at(i);
        if (!client || client->SocketClients == socket) {
            delete client;
            softPhoneSocketClient.removeAt(i);
        }
    }

    socket->deleteLater();

    if (m_clients.isEmpty()) {
        enableDataLogger = false;
        qInfo() << "[ChatServerWebRec] enableDataLogger = FALSE (no clients)";
    }

    emit clientDisconnected(socket);
    emit clientCountChanged(m_clients.size());
}

void ChatServerWebRec::broadcastMessage(const QString &message)
{
    qDebug() << "[ChatServerWebRec] broadcast:" << message.left(200)
    << "to" << m_clients.size() << "clients";

    const auto clientsCopy = m_clients;
    for (QWebSocket *sock : clientsCopy) {
        if (!sock || sock->state() != QAbstractSocket::ConnectedState)
            continue;
        sock->sendTextMessage(message);
    }
}

void ChatServerWebRec::sendMessageTo(QWebSocket *client, const QString &message)
{
    if (!client) {
        qWarning() << "[ChatServerWebRec] sendMessageTo: null client";
        return;
    }
    if (!m_clients.contains(client)) {
        qWarning() << "[ChatServerWebRec] sendMessageTo: client not in list";
        return;
    }
    if (client->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "[ChatServerWebRec] sendMessageTo: client not connected";
        return;
    }

    client->sendTextMessage(message);
}

void ChatServerWebRec::sendMessageToRecService(QString message, int softPhoneID)
{
    // qDebug() <<  "sendMessageToRecService recSocketClient" << recSocketClient.length() << softPhoneID;
    Q_FOREACH (SoftPhoneSocketClient *sClient, recSocketClient)
    {

        if (sClient && sClient->softPhoneID == softPhoneID
                && sClient->SocketClients
                && sClient->SocketClients->state() == QAbstractSocket::ConnectedState)
        {
            sClient->SocketClients->sendTextMessage(message);
        }
    }
}
void ChatServerWebRec::sendSquelchStatus(int softPhoneID, bool pttOn, bool sqlOn, bool callState, QString state, double freq)
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
    // qDebug() <<"sendSquelchStatus:jsonString" << jsonString;
    sendMessageToRecService(jsonString,softPhoneID);
}
void ChatServerWebRec::sendToWebMessageClientWebSender(QString message,QWebSocket *webClient)
{
    qDebug() <<"sendSquelchStatus:jsonString_Webrec" << message;
    if (!webClient || webClient->state() != QAbstractSocket::ConnectedState)
        return;
    Q_FOREACH (QWebSocket *client, m_WebSocketClients)
    {
        if (client == webClient) {
            webClient->sendTextMessage(message);
            return;
        }
    }
}
void ChatServerWebRec::updateAllowedUri(uint8_t softPhoneID, uint8_t numConn, QString uri1, QString uri2, QString uri3, QString uri4, QString uri5, QString uri6, QString uri7, QString uri8)
{
    QString message = QString("{\"menuID\":\"uriAllowedList\", \"numConn\":%1, \"uri1\":\"%2\", \"uri2\":\"%3\", \"uri3\":\"%4\", \"uri4\":\"%5\", \"uri5\":\"%6\", \"uri6\":\"%7\", \"uri7\":\"%8\", \"uri8\":\"%9\", \"softPhoneID\":%10}")
    .arg(numConn).arg(uri1).arg(uri2).arg(uri3).arg(uri4).arg(uri5).arg(uri6).arg(uri7).arg(uri8).arg(softPhoneID);
    const auto clientsCopy = m_WebSocketClients;
    for (QWebSocket *client : clientsCopy)
    {
        if (client && client->state() == QAbstractSocket::ConnectedState)
            client->sendTextMessage(message);
    }
}
