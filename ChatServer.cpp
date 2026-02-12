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
#include "ChatServer.h"
#include <QDateTime>

QT_USE_NAMESPACE

ChatServer::ChatServer(quint16 port, QObject *parent) :
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
                this, &ChatServer::onNewConnection);
    }
}

ChatServer::~ChatServer()
{
    m_pWebSocketServer->close();
    qDeleteAll(m_clients.begin(), m_clients.end());
}

void ChatServer::onNewConnection()
{
    QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();
    connect(pSocket, &QWebSocket::textMessageReceived, this, &ChatServer::processMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &ChatServer::socketDisconnected);
    m_clients << pSocket;
    qDebug() << "On New Connection from address : " << pSocket->peerName();
    emit onNewClientConneced(pSocket);
    if (clientNum <= 0)
    {
        clientNum = m_clients.length();
        emit onNumClientChanged(clientNum);
    }
    else {
        clientNum = m_clients.length();
    }

}

void ChatServer::broadcastMessage(QString message){
    Q_FOREACH (QWebSocket *pClient, m_clients)
    {
        pClient->sendTextMessage(message);
    }    
}

void ChatServer::commandProcess(QString message, QWebSocket *pSender){
//    message = message.replace(" ","");
    QJsonDocument d = QJsonDocument::fromJson(message.toUtf8());
    QJsonObject command = d.object();
    QString getCommand =  QJsonValue(command["menuID"]).toString();
    QString objectName =  QJsonValue(command["objectName"]).toString();

   // qDebug() << "commandProcess:getCommand:" << objectName << getCommand;
    if ((objectName != "") || (getCommand != "")){
        emit newCommandProcess(command, pSender, message);

        if (getCommand == "getSystemPage"){
            qDebug() << "getSystemPage:" << objectName << getCommand;
            m_WebSocketClients << pSender;
            emit newSettingPageConnectd(pSender);
        }
    }
    if (getCommand == "register")
    {
         qDebug() << "register_message:" << message;
        int softPhoneID = QJsonValue(command["iGateID"]).toInt();
        SoftPhoneSocketClient *sClient = new SoftPhoneSocketClient;
        sClient->softPhoneID = softPhoneID;
        sClient->SocketClients = pSender;
        recSocketClient.append(sClient);
        qDebug() <<  "append recSocketClient" << recSocketClient.length();
        qDebug() << "register_message:" << message;
    }
    else if (getCommand == "applyRecSettings") {
        handleApplyRecSettings(command);
    }
    else if (getCommand == ("recLogging"))
    {
        qDebug() <<  "append recSocketClient recLogging:" << recSocketClient.length() << message;
        int softPhoneID = QJsonValue(command["iGateID"]).toInt();
        int recorderID = QJsonValue(command["iGateID"]).toInt();
        QString recState = QJsonValue(command["state"]).toString();
        if (softPhoneID >4) softPhoneID = softPhoneID-4;
        emit recLogging(softPhoneID,recorderID,recState,message);

    }else if (getCommand == ("getVuMeter"))
    {
        m_WebSocketVUClients << pSender;
        emit getVuMeter(pSender);
    }
    else {
        emit newCommandProcess(command, pSender, message);
        // qDebug() << "getCommand" << getCommand << message;
    }
}

void ChatServer::processMessage(QString message)
{
    // qDebug() << "processMessage" << message;
    QWebSocket *pSender = qobject_cast<QWebSocket *>(sender());
    commandProcess(message, pSender);

}

void ChatServer::socketDisconnected()
{
//    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
//    if (pClient)
//    {
//        m_clients.removeAll(pClient);
//        m_snmpSocketClients.removeAll(pClient);
//        m_WebSocketClients.removeAll(pClient);
//        pClient->deleteLater();
//        qDebug() << pClient->localAddress().toString() << "has disconect";
//    }
//    clientNum = m_clients.length();
//    if (clientNum <= 0)
//    {
//        emit onNumClientChanged(clientNum);
//    }
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (!pClient) return;

    qDebug().noquote()
        << "[WS DISCONNECT]"
        << "peer=" << pClient->peerAddress().toString() << pClient->peerPort()
        << "local=" << pClient->localAddress().toString() << pClient->localPort()
        << "recSocketClient(before)=" << recSocketClient.size()
        << "m_WebSocketRecClients(before)=" << m_WebSocketRecClients.size();

    // ✅ 1) remove from plain socket lists (QList<QWebSocket*>)
    m_clients.removeAll(pClient);
    m_WebSocketClients.removeAll(pClient);
    m_WebSocketVUClients.removeAll(pClient);
    m_WebSocketRecClients.removeAll(pClient);

    // ✅ 2) remove from your wrapper lists (delete wrapper ONLY, not socket)
    for (int i = softPhoneSocketClient.size() - 1; i >= 0; --i) {
        auto c = softPhoneSocketClient[i];
        if (!c) { softPhoneSocketClient.removeAt(i); continue; }
        if (c->SocketClients == pClient) {
            delete c;
            softPhoneSocketClient.removeAt(i);
        }
    }

    for (int i = igateGroupMngSocketClient.size() - 1; i >= 0; --i) {
        auto c = igateGroupMngSocketClient[i];
        if (!c) { igateGroupMngSocketClient.removeAt(i); continue; }
        if (c->SocketClients == pClient) {
            delete c;
            igateGroupMngSocketClient.removeAt(i);
        }
    }

    for (int i = recSocketClient.size() - 1; i >= 0; --i) {
        auto c = recSocketClient[i];
        if (!c) { recSocketClient.removeAt(i); continue; }

        if (c->SocketClients == pClient) {
            qDebug() << "[WS DISCONNECT] remove recSocketClient softPhoneID=" << c->softPhoneID;
            delete c;                   // ✅ delete wrapper
            recSocketClient.removeAt(i);
        }
    }

    // ✅ 3) IMPORTANT: DO NOT delete socket here (avoid double-delete / timing issues)
    // pClient->deleteLater();   ❌ remove this line

    qDebug().noquote()
        << "[WS DISCONNECT DONE]"
        << "recSocketClient(after)=" << recSocketClient.size()
        << "m_WebSocketRecClients(after)=" << m_WebSocketRecClients.size();
}
QString ChatServer::wsInfo(QWebSocket *s)
{
    if (!s) return "null";
    return QString("sock=%1 local=%2:%3 peer=%4:%5 state=%6")
        .arg(reinterpret_cast<quintptr>(s), 0, 16)
        .arg(s->localAddress().toString())
        .arg(s->localPort())
        .arg(s->peerAddress().toString())
        .arg(s->peerPort())
        .arg(int(s->state()));
}
void ChatServer::sendToWebMessageClient(QString message)
{
    // qDebug().noquote()
    //     << "sendToWebMessageClient:"
    //     << "len(Web)=" << m_WebSocketClients.length()
    //     << "len(VU)="  << m_WebSocketVUClients.length()
    //     << "len(All)=" << m_clients.length()
    //     << "msg=" << message;

    // qDebug().noquote() << "---- m_clients ----";
    for (QWebSocket *c : m_clients) //qDebug().noquote() << wsInfo(c);

    // qDebug().noquote() << "---- m_WebSocketClients ----";
    for (QWebSocket *c : m_WebSocketClients) //qDebug().noquote() << wsInfo(c);

    // qDebug().noquote() << "---- m_WebSocketVUClients ----";
    for (QWebSocket *c : m_WebSocketVUClients) //qDebug().noquote() << wsInfo(c);

    // send web clients
    for (QWebSocket *webClient : m_WebSocketClients) {
        if (!webClient || webClient->state() != QAbstractSocket::ConnectedState) {
            // qDebug().noquote() << "[SKIP][Web]" << wsInfo(webClient);
            continue;
        }
        webClient->sendTextMessage(message);
        // qDebug().noquote() << "[SEND][Web]" << wsInfo(webClient);
    }

    // send vu clients
    for (QWebSocket *vuClient : m_WebSocketVUClients) {
        if (!vuClient || vuClient->state() != QAbstractSocket::ConnectedState) {
            // qDebug().noquote() << "[SKIP][VU]" << wsInfo(vuClient);
            continue;
        }
        vuClient->sendTextMessage(message);
        // qDebug().noquote() << "[SEND][VU]" << wsInfo(vuClient);
    }
}
void ChatServer::sendSquelchStatus(int softPhoneID, bool pttOn, bool sqlOn,
                                  bool callState, QString state, double freq)
{
    Q_UNUSED(callState)
    Q_UNUSED(state)

    QJsonObject message;
    message["iGateID"]  = softPhoneID;
    message["object"]   = "receiverStatus";
    message["state"]    = state;
    message["squelch"]  = sqlOn ? "on" : "off";
    message["pttOn"]    = pttOn ? "on" : "off";
    message["device"]   = QString("recin%1").arg(softPhoneID <= 4 ? softPhoneID : softPhoneID - 4);
    message["recorder"] = "enable";
    qint64 freqHz = 0;
    if (freq > 0.0 && freq < 1000000.0) {
        freqHz = (qint64)qRound64(freq * 1e6);
    } else {
        freqHz = (qint64)qRound64(freq);
    }
    message["frequency"] = freqHz;
    QJsonDocument doc(message);
    QString jsonString = doc.toJson(QJsonDocument::Compact);
    qDebug() << "sendMessageToRecService_Debug" << jsonString
             << "freq_in=" << freq << "freqHz=" << freqHz;
    sendMessageToRecService(jsonString, softPhoneID);
}
void ChatServer::sendMessageToRecService(const QString& message, int softPhoneID)
{
    qDebug().noquote()
        << "[SEND to REC]"
        << "this=" << (void*)this
        << "listenPort=" << m_listenPort
        << "clients=" << recSocketClient.size()
        << "target=" << softPhoneID;

    // ✅ copy กัน list เปลี่ยนระหว่าง loop (disconnect อาจ remove)
    const auto clientsCopy = recSocketClient;

    for (SoftPhoneSocketClient* sClient : clientsCopy)
    {
        if (!sClient) continue;
        if (sClient->softPhoneID != softPhoneID) continue;

        QWebSocket* ws = sClient->SocketClients;   // สมมติ SocketClients เป็น QWebSocket*
        if (!ws) {
            qWarning() << "[SEND to REC] ws is null softPhoneID=" << softPhoneID;
            continue;
        }

        // ✅ ถ้าไม่ connected ก็ไม่ส่ง
        if (ws->state() != QAbstractSocket::ConnectedState) {
            qWarning() << "[SEND to REC] not connected state=" << ws->state()
                       << "softPhoneID=" << softPhoneID;
            continue;
        }

        // ✅ ส่งใน thread ของ ws เสมอ
        const QString msg = message;

        if (ws->thread() != QThread::currentThread())
        {
            QMetaObject::invokeMethod(ws, [ws, msg]() {
                if (ws && ws->state() == QAbstractSocket::ConnectedState)
                    ws->sendTextMessage(msg);
            }, Qt::QueuedConnection);
        }
        else
        {
            ws->sendTextMessage(msg);
        }

        return;
    }

    qWarning() << "[SEND to REC] target softPhoneID not found:" << softPhoneID;
}
