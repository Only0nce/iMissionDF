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

    // Transport truth only: this log follows QTcpSocket state transitions and
    // never changes the UI state by itself. It is intentionally verbose enough
    // to diagnose LAN3/LAN4 control connectivity on the target.
    connect(&m_socket, &QTcpSocket::stateChanged, this, [this](QAbstractSocket::SocketState state) {
        const char *name = "Unknown";
        switch (state) {
        case QAbstractSocket::UnconnectedState: name = "Unconnected"; break;
        case QAbstractSocket::HostLookupState:  name = "HostLookup"; break;
        case QAbstractSocket::ConnectingState:  name = "Connecting"; break;
        case QAbstractSocket::ConnectedState:   name = "Connected"; break;
        case QAbstractSocket::BoundState:       name = "Bound"; break;
        case QAbstractSocket::ListeningState:   name = "Listening"; break;
        case QAbstractSocket::ClosingState:     name = "Closing"; break;
        }
        qInfo().noquote() << "[LAN][RFSoC-TCP][STATE]"
                          << name
                          << QStringLiteral("%1:%2").arg(m_lastHost).arg(m_lastPort);
    });

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
    // NET-ENDPOINTS2.1: restore the proven RFSoC control recovery model.
    // Keep one periodic watchdog while disconnected instead of a single-shot
    // exponential timer that can lose recovery after a transient state race.
    m_reconnectTimer.setSingleShot(false);
    connect(&m_reconnectTimer, &QTimer::timeout,
            this, &TcpClientDF::attemptReconnect);

    // ---- heartbeat timer ----
    m_heartbeatTimer.setInterval(m_heartbeatMs);
    m_heartbeatTimer.setSingleShot(false);
    connect(&m_heartbeatTimer, &QTimer::timeout,
            this, &TcpClientDF::sendHeartbeat);
}


TcpClientDF::~TcpClientDF()
{
    m_shuttingDown = true;
    m_userDisconnect = true;
    m_reconnectTimer.stop();
    m_heartbeatTimer.stop();
    m_pendingWrites.clear();
    m_socket.abort();
}

void TcpClientDF::setReconnectIntervalMs(int ms)
{
    if (ms < 200)
        ms = 200;
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
    const QString cleanHost = host.trimmed();
    if (cleanHost.isEmpty() || port == 0) {
        qWarning() << "[LAN][RFSoC-TCP] refused invalid target" << cleanHost << port;
        return;
    }

    m_shuttingDown = false;
    m_userDisconnect = false;

    const bool sameTarget = (m_lastHost == cleanHost && m_lastPort == port);
    const auto state = m_socket.state();

    // Do not tear down a healthy TCP session just because the same endpoint is
    // replayed from DB/UI. This was a major source of misleading disconnect
    // transitions while opening Network/Endpoints pages.
    if (sameTarget && state == QAbstractSocket::ConnectedState) {
        qInfo().noquote() << "[LAN][RFSoC-TCP] already connected"
                          << QStringLiteral("%1:%2").arg(cleanHost).arg(port);
        return;
    }

    m_lastHost = cleanHost;
    m_lastPort = port;
    m_immediateRetryConsumed = false;
    m_rxSeenThisSession = false;

    emit logMessage(QString("Connecting to %1:%2").arg(cleanHost).arg(port));
    qInfo().noquote() << "[LAN][RFSoC-TCP] connectToHost"
                      << QStringLiteral("%1:%2").arg(cleanHost).arg(port);

    m_buffer.clear();

    // Proven R-LAN4B.1 recovery model: keep a periodic watchdog armed while
    // disconnected/connecting. onConnected() stops it immediately.
    if (!m_reconnectTimer.isActive())
        m_reconnectTimer.start();

    // A new target must replace any stale in-flight connection. Replaying the
    // same target while Connecting is left to the watchdog instead of causing
    // repeated abort/connect churn.
    if (!(sameTarget && state == QAbstractSocket::ConnectingState)) {
        m_socket.abort();
        m_socket.connectToHost(cleanHost, port);
    }
}

void TcpClientDF::disconnectFromServer()
{
    m_userDisconnect = true;
    m_reconnectTimer.stop();
    m_heartbeatTimer.stop();
    m_pendingWrites.clear();

    if (m_socket.state() == QAbstractSocket::UnconnectedState)
        return;
    m_socket.disconnectFromHost();
}

void TcpClientDF::onConnected()
{
    emit logMessage("Connected to DoA server");
    qInfo().noquote() << "[LAN][RFSoC-TCP] connected"
                      << QStringLiteral("%1:%2").arg(m_lastHost).arg(m_lastPort);
    emit connected();
    m_userDisconnect = false;
    m_immediateRetryConsumed = false;
    m_rxSeenThisSession = false;

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
    qWarning().noquote() << "[LAN][RFSoC-TCP] disconnected"
                         << QStringLiteral("%1:%2").arg(m_lastHost).arg(m_lastPort);
    emit disconnected();

    // stop heartbeat
    if (m_heartbeatTimer.isActive())
        m_heartbeatTimer.stop();

    if (!m_userDisconnect && !m_shuttingDown) {
        if (!m_reconnectTimer.isActive())
            m_reconnectTimer.start();

        // One immediate retry per outage, then the repeating watchdog owns
        // recovery. This preserves the working legacy behaviour without a
        // tight reconnect loop.
        if (!m_immediateRetryConsumed) {
            m_immediateRetryConsumed = true;
            QMetaObject::invokeMethod(this, "attemptReconnect", Qt::QueuedConnection);
        }
    }
}

void TcpClientDF::onReadyRead()
{
    const QByteArray incoming = m_socket.readAll();
    if (!incoming.isEmpty() && !m_rxSeenThisSession) {
        m_rxSeenThisSession = true;
        qInfo().noquote() << "[LAN][RFSoC-TCP][RX] first server data"
                          << "bytes=" << incoming.size()
                          << QStringLiteral("target=%1:%2").arg(m_lastHost).arg(m_lastPort);
    }
    m_buffer.append(incoming);

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
    qWarning().noquote() << "[LAN][RFSoC-TCP] socket error"
                         << QStringLiteral("%1:%2").arg(m_lastHost).arg(m_lastPort)
                         << err;

    // stop heartbeat
    if (m_heartbeatTimer.isActive())
        m_heartbeatTimer.stop();

    if (!m_userDisconnect && !m_shuttingDown) {
        if (!m_reconnectTimer.isActive())
            m_reconnectTimer.start();

        if (!m_immediateRetryConsumed) {
            m_immediateRetryConsumed = true;
            QMetaObject::invokeMethod(this, "attemptReconnect", Qt::QueuedConnection);
        }
    }
}

void TcpClientDF::scheduleReconnect(const QString &reason)
{
    Q_UNUSED(reason);
    if (m_shuttingDown || m_userDisconnect || m_lastHost.isEmpty() || m_lastPort == 0)
        return;
    if (m_socket.state() == QAbstractSocket::ConnectedState)
        return;
    if (!m_reconnectTimer.isActive())
        m_reconnectTimer.start();
}

void TcpClientDF::requestReconnect(const QString &reason, bool allowImmediate)
{
    if (m_shuttingDown || m_userDisconnect || m_lastHost.isEmpty() || m_lastPort == 0)
        return;
    if (m_socket.state() == QAbstractSocket::ConnectedState)
        return;

    scheduleReconnect(reason);

    if (allowImmediate && !m_immediateRetryConsumed) {
        m_immediateRetryConsumed = true;
        qWarning().noquote() << "[LAN][RFSoC-TCP] immediate reconnect queued"
                             << QStringLiteral("%1:%2").arg(m_lastHost).arg(m_lastPort)
                             << "reason=" << reason;
        QMetaObject::invokeMethod(this, "attemptReconnect", Qt::QueuedConnection);
    }
}

void TcpClientDF::attemptReconnect()
{
    if (m_shuttingDown || m_userDisconnect || m_lastHost.isEmpty() || m_lastPort == 0)
        return;

    const auto state = m_socket.state();

    if (state == QAbstractSocket::ConnectedState) {
        if (m_reconnectTimer.isActive())
            m_reconnectTimer.stop();
        return;
    }

    if (state == QAbstractSocket::ConnectingState) {
        emit logMessage("Reconnect: connecting timeout -> abort & retry");
        qWarning().noquote() << "[LAN][RFSoC-TCP] connecting watchdog retry"
                             << QStringLiteral("%1:%2").arg(m_lastHost).arg(m_lastPort);
        m_socket.abort();
    }

    emit logMessage(QString("Reconnecting to %1:%2 ...").arg(m_lastHost).arg(m_lastPort));
    qInfo().noquote() << "[LAN][RFSoC-TCP] reconnect attempt"
                      << QStringLiteral("%1:%2").arg(m_lastHost).arg(m_lastPort);

    m_socket.abort();
    m_socket.connectToHost(m_lastHost, m_lastPort);

    if (!m_reconnectTimer.isActive())
        m_reconnectTimer.start();
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
        requestReconnect(QStringLiteral("heartbeat-write"), true);
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

bool TcpClientDF::sendLineIfConnected(const QByteArray &line, bool addNewline)
{
    QByteArray payload = line;
    if (addNewline && !payload.endsWith('\n'))
        payload.append('\n');

    // Authoritative remote-LAN command path: never queue a setIpConfig packet.
    // Either the RFSoC TCP session is connected right now and the bytes are
    // accepted by QTcpSocket::write(), or the command is rejected.
    if (m_socket.state() != QAbstractSocket::ConnectedState)
        return false;

    const qint64 n = m_socket.write(payload);
    if (n != payload.size()) {
        const QString err = (n < 0)
            ? m_socket.errorString()
            : QStringLiteral("partial socket write %1/%2 bytes").arg(n).arg(payload.size());
        emit logMessage(QString("write() failed: %1").arg(err));
        m_socket.abort();
        requestReconnect(QStringLiteral("write-failed"), true);
        return false;
    }

    return true;
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
            requestReconnect(QStringLiteral("write-failed"), true);
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

    // Generic DoA/control traffic retains the legacy deferred-write queue.
    // Safety-critical LAN3/LAN4 setIpConfig commands use sendLineIfConnected()
    // instead and therefore never enter this queue.
    if (hasTarget())
        requestReconnect(QStringLiteral("queued-write"), true);

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
            requestReconnect(QStringLiteral("flush-write-failed"), true);
            break;
        }
    }
}
