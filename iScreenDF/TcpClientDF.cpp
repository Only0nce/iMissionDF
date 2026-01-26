// =============================== TcpClientDF.cpp ===============================
#include "TcpClientDF.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QDateTime>
#include <QDebug>

TcpClientDF::TcpClientDF(QObject *parent)
    : QObject(parent)
{
    // ---- socket signals ----
    connect(&m_socket, &QTcpSocket::connected,
            this, &TcpClientDF::onConnected);

    connect(&m_socket, &QTcpSocket::disconnected,
            this, &TcpClientDF::onDisconnected);

    connect(&m_socket, &QTcpSocket::readyRead,
            this, &TcpClientDF::onReadyRead);

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(&m_socket, &QTcpSocket::errorOccurred,
            this, &TcpClientDF::onError);
#else
    connect(&m_socket,
            SIGNAL(error(QAbstractSocket::SocketError)),
            this,
            SLOT(onError(QAbstractSocket::SocketError)));
#endif

    // ---- reconnect timer ----
    m_reconnectTimer.setInterval(m_reconnectMs);
    m_reconnectTimer.setSingleShot(false);
    connect(&m_reconnectTimer, &QTimer::timeout,
            this, &TcpClientDF::attemptReconnect);

    // ---- heartbeat timer ----
    m_heartbeatTimer.setInterval(m_heartbeatMs);
    m_heartbeatTimer.setSingleShot(false);
    connect(&m_heartbeatTimer, &QTimer::timeout,
            this, &TcpClientDF::sendHeartbeat);
}

void TcpClientDF::setReconnectIntervalMs(int ms)
{
    if (ms < 200) ms = 200;
    m_reconnectMs = ms;
    m_reconnectTimer.setInterval(m_reconnectMs);
}

void TcpClientDF::setHeartbeatIntervalMs(int ms)
{
    if (ms < 200) ms = 200;
    m_heartbeatMs = ms;
    m_heartbeatTimer.setInterval(m_heartbeatMs);
}

void TcpClientDF::setHeartbeatEnabled(bool en)
{
    m_heartbeatEnabled = en;
    if (!m_heartbeatEnabled && m_heartbeatTimer.isActive())
        m_heartbeatTimer.stop();
    else if (m_heartbeatEnabled && m_socket.state() == QAbstractSocket::ConnectedState && !m_heartbeatTimer.isActive())
        m_heartbeatTimer.start();
}

void TcpClientDF::connectToServer(const QString &host, quint16 port)
{
    m_lastHost = host;
    m_lastPort = port;

    emit logMessage(QString("Connecting to %1:%2").arg(host).arg(port));

    // reset any stale buffers
    m_buffer.clear();

    // ensure reconnect timer runs
    if (!m_reconnectTimer.isActive())
        m_reconnectTimer.start();

    // connect now
    m_socket.abort();
    m_socket.connectToHost(host, port);
}

void TcpClientDF::disconnectFromServer()
{
    // stop timers
    if (m_reconnectTimer.isActive())
        m_reconnectTimer.stop();
    if (m_heartbeatTimer.isActive())
        m_heartbeatTimer.stop();

    m_socket.disconnectFromHost();
}

void TcpClientDF::onConnected()
{
    emit logMessage("Connected to DoA server");
    emit connected();

    // stop reconnect attempts while connected
    if (m_reconnectTimer.isActive())
        m_reconnectTimer.stop();

    // start heartbeat
    if (m_heartbeatEnabled && !m_heartbeatTimer.isActive())
        m_heartbeatTimer.start();

    // flush queued outgoing messages
    flushPendingWrites();
}

void TcpClientDF::onDisconnected()
{
    emit logMessage("Disconnected from DoA server");
    emit disconnected();

    // stop heartbeat
    if (m_heartbeatTimer.isActive())
        m_heartbeatTimer.stop();

    // kick reconnect immediately (don’t wait full interval)
    QMetaObject::invokeMethod(this, "attemptReconnect", Qt::QueuedConnection);

    if (!m_reconnectTimer.isActive())
        m_reconnectTimer.start();
}

void TcpClientDF::onReadyRead()
{
    m_buffer.append(m_socket.readAll());

    while (true) {
        int idx = m_buffer.indexOf('\n');
        if (idx < 0)
            break;

        QByteArray line = m_buffer.left(idx);
        m_buffer.remove(0, idx + 1);

        if (!line.trimmed().isEmpty())
            processLine(line);
    }
}

void TcpClientDF::onError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)

    const QString err = m_socket.errorString();
    emit errorOccurred(err);
    emit logMessage(QString("Socket error: %1").arg(err));

    // stop heartbeat
    if (m_heartbeatTimer.isActive())
        m_heartbeatTimer.stop();

    // trigger reconnect immediately
    QMetaObject::invokeMethod(this, "attemptReconnect", Qt::QueuedConnection);

    if (!m_reconnectTimer.isActive())
        m_reconnectTimer.start();
}

void TcpClientDF::attemptReconnect()
{
    if (m_lastHost.isEmpty() || m_lastPort == 0)
        return;

    auto st = m_socket.state();

    if (st == QAbstractSocket::ConnectedState) {
        // already connected
        if (m_reconnectTimer.isActive())
            m_reconnectTimer.stop();
        if (m_heartbeatEnabled && !m_heartbeatTimer.isActive())
            m_heartbeatTimer.start();
        return;
    }

    if (st == QAbstractSocket::ConnectingState) {
        // IMPORTANT: connecting can get stuck → abort and retry
        emit logMessage("Reconnect: connecting stuck -> abort & retry");
        m_socket.abort();
        // continue to connectToHost below
    }

    emit logMessage(QString("Reconnecting to %1:%2 ...").arg(m_lastHost).arg(m_lastPort));

    m_socket.abort();
    m_socket.connectToHost(m_lastHost, m_lastPort);
}

void TcpClientDF::sendHeartbeat()
{
    if (!m_heartbeatEnabled)
        return;

    if (m_socket.state() != QAbstractSocket::ConnectedState)
        return;

    // ✅ Keep alive packet (JSON + newline)
    // If your server does NOT like JSON ping, replace with: QByteArray payload = "ping\n";
    QJsonObject ping;
    ping["menuID"] = "ping";
    ping["ts"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QByteArray payload = QJsonDocument(ping).toJson(QJsonDocument::Compact);
    if (!payload.endsWith('\n'))
        payload.append('\n');

    qint64 n = m_socket.write(payload);
    if (n < 0) {
        emit logMessage(QString("[Heartbeat] write failed: %1").arg(m_socket.errorString()));
        // force disconnect so reconnect logic kicks in
        m_socket.abort();
        QMetaObject::invokeMethod(this, "attemptReconnect", Qt::QueuedConnection);
        return;
    }

    // no waitForBytesWritten() to avoid blocking
    // emit logMessage("[Heartbeat] ping sent"); // enable if you want spam logs
}

void TcpClientDF::processLine(const QByteArray &line)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (err.error != QJsonParseError::NoError) {
        emit logMessage(QString("JSON parse error: %1").arg(err.errorString()));
        return;
    }

    if (!doc.isObject()) {
        emit logMessage("Invalid JSON object");
        return;
    }

    updateFromJson(doc.object());
}

void TcpClientDF::updateFromJson(const QJsonObject &obj)
{
    emit doaResultReceived(obj);
    emit updateFromTcpServer(obj);
}

bool TcpClientDF::sendJson(const QJsonObject &obj, bool addNewline)
{
    QJsonDocument doc(obj);
    QByteArray line = doc.toJson(QJsonDocument::Compact);
    return sendLine(line, addNewline);
}

bool TcpClientDF::sendLine(const QByteArray &line, bool addNewline)
{
    QByteArray payload = line;
    if (addNewline && !payload.endsWith('\n'))
        payload.append('\n');

    if (m_socket.state() == QAbstractSocket::ConnectedState) {
        qint64 n = m_socket.write(payload);
        if (n < 0) {
            emit logMessage(QString("write() failed: %1").arg(m_socket.errorString()));
            // force disconnect and let reconnect happen
            m_socket.abort();
            QMetaObject::invokeMethod(this, "attemptReconnect", Qt::QueuedConnection);
            return false;
        }
        return true;
    }

    // not connected → queue it (drop oldest if full)
    if (m_pendingWrites.size() >= m_maxPending) {
        // drop 1 oldest
        m_pendingWrites.pop_front();
        emit logMessage("Queue overflow - dropping 1 chunks");
    }

    m_pendingWrites.push_back(payload);
    // emit logMessage("Not connected; queued outgoing message"); // enable if you want
    return false;
}

void TcpClientDF::flushPendingWrites()
{
    if (m_socket.state() != QAbstractSocket::ConnectedState)
        return;

    while (!m_pendingWrites.isEmpty()) {
        QByteArray payload = m_pendingWrites.front();
        m_pendingWrites.pop_front();

        qint64 n = m_socket.write(payload);
        if (n < 0) {
            emit logMessage(QString("flush write() failed: %1").arg(m_socket.errorString()));
            // force disconnect and reconnect later
            m_socket.abort();
            QMetaObject::invokeMethod(this, "attemptReconnect", Qt::QueuedConnection);
            break;
        }
    }
}
