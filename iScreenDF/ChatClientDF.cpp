#include "ChatClientDF.h"

QT_USE_NAMESPACE

ChatClientDF::ChatClientDF(QObject *parent)
    : QObject(parent), isConnected(false)
{
    m_webSocket = new QWebSocket();
    connectSignals();

    m_reconnectTimer.setInterval(5000); // retry every 5 seconds
    m_reconnectTimer.setSingleShot(false); // run forever
    connect(&m_reconnectTimer, &QTimer::timeout, this, &ChatClientDF::ensureConnected);
    // m_reconnectTimer.start();
}
void ChatClientDF::setUniqueIdInGroup(const QString &id) {
    m_uniqueIdInGroup = id;
}

void ChatClientDF::connectSignals()
{
    connect(m_webSocket, &QWebSocket::connected, this, &ChatClientDF::onConnected);
    connect(m_webSocket, &QWebSocket::disconnected, this, &ChatClientDF::onDisconnected);
    connect(m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &ChatClientDF::onError);
    connect(m_webSocket, &QWebSocket::textMessageReceived,
            this, &ChatClientDF::onTextMessageReceived);
}

void ChatClientDF::sendTextMessage(const QString &message)
{
    if (isConnected && m_webSocket->state() == QAbstractSocket::ConnectedState) {
        m_webSocket->sendTextMessage(message);
        if (m_debug)
            qDebug() << "Sent message:" << message;
    } else {
        qWarning() << "Cannot send message, socket not connected.";
    }
}

void ChatClientDF::createConnection(const QString &ipaddress, const quint16 &port)
{
    m_ipaddress = ipaddress;
    m_port      = port;
    m_socketID  = m_groupIndex;
    // m_socketID = static_cast<int>(port);

    m_url = QUrl(QString("ws://%1:%2").arg(ipaddress).arg(port));

    if (m_debug)
        qDebug() << "WebSocket server:" << m_url.toString();

    m_webSocket->open(m_url);

    if (!m_reconnectTimer.isActive())
        m_reconnectTimer.start();
}


void ChatClientDF::ensureConnected()
{
    if (m_ipaddress.isEmpty() || m_port == 0) {
        if (m_debug)
            qDebug() << "ensureConnected: server address not set yet, skip reconnect";
        return;
    }

    if (!isConnected || m_webSocket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << " Auto-reconnect triggered";
        reconnect();
    }
}

void ChatClientDF::setServerAddress(const QString &host, quint16 port) {
    m_host = host;
    m_port = port;
}

void ChatClientDF::reconnect2() {
    if (m_webSocket->state() != QAbstractSocket::UnconnectedState) {
        m_webSocket->close(); // ตัดการเชื่อมต่อเดิม
    }
    m_webSocket->open(QUrl(QStringLiteral("ws://%1:%2").arg(m_host).arg(m_port))); // เชื่อมใหม่
}

void ChatClientDF::reconnect()
{
    if (m_webSocket) {
        m_webSocket->abort();
        m_webSocket->deleteLater();
    }

    m_webSocket = new QWebSocket();
    connectSignals();

    QUrl url(QString("ws://%1:%2").arg(m_ipaddress).arg(m_port));
    if (m_debug)
        qDebug() << "Reconnecting to" << url.toString();

    m_webSocket->open(url);
}


void ChatClientDF::onConnected() {
    isConnected = true;
    if (m_debug)
        qDebug() << "WebSocket connected" << m_ipaddress;

    if (!m_uniqueIdInGroup.isEmpty()) {
        emit onDeviceConnected(m_uniqueIdInGroup, m_ipaddress);
        qDebug() << "WebSocket connected uniqueIdInGroup=" << m_uniqueIdInGroup
                 << "ip=" << m_ipaddress;
    }
}

void ChatClientDF::onTextMessageReceived(const QString &message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    QJsonObject obj = doc.object();
    emit TextMessageReceived(message);
}

void ChatClientDF::onDisconnected()
{
    isConnected = false;
    qDebug() << m_ipaddress << "WebSocket disconnected";
    emit closed(m_socketID, m_ipaddress);
}

void ChatClientDF::onError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    qDebug() << "WebSocket error:" << m_webSocket->errorString();
    if (isConnected) {
        emit closed(m_socketID, m_ipaddress);
        isConnected = false;
    }
}

void ChatClientDF::disconnectFromServer()
{
    if (m_reconnectTimer.isActive())
        m_reconnectTimer.stop();

    if (m_webSocket) {
        if (m_debug)
            qDebug() << "[ChatClientDF] disconnectFromServer() -> close socket";
        m_webSocket->close();
    }
}

void ChatClientDF::stopReconnectTimer()
{
    if (m_reconnectTimer.isActive())
        m_reconnectTimer.stop();
}
