// =============================== TcpClientDF.cpp ===============================
#include "TcpClientDF.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QDateTime>
#include <QDebug>
#include <QMetaObject>
#include <QtGlobal>

TcpClientDF::TcpClientDF(QObject *parent)
    : QObject(parent)
{
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

    // Single-shot watchdog/backoff avoids the historical reconnect storm while
    // still recovering a ConnectingState that never completes.
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout,
            this, &TcpClientDF::attemptReconnect);

    m_heartbeatTimer.setInterval(m_heartbeatMs);
    m_heartbeatTimer.setSingleShot(false);
    connect(&m_heartbeatTimer, &QTimer::timeout,
            this, &TcpClientDF::sendHeartbeat);
}

void TcpClientDF::setReconnectIntervalMs(int ms)
{
    if (ms < 200)
        ms = 200;

    m_reconnectMs = ms;
    m_reconnectCurrentMs = ms;
    if (m_reconnectMaxMs < m_reconnectMs)
        m_reconnectMaxMs = m_reconnectMs;
}

void TcpClientDF::setHeartbeatIntervalMs(int ms)
{
    if (ms < 200)
        ms = 200;
    m_heartbeatMs = ms;
    m_heartbeatTimer.setInterval(m_heartbeatMs);
}

void TcpClientDF::setHeartbeatEnabled(bool en)
{
    m_heartbeatEnabled = en;
    if (!m_heartbeatEnabled && m_heartbeatTimer.isActive()) {
        m_heartbeatTimer.stop();
    } else if (m_heartbeatEnabled &&
               m_socket.state() == QAbstractSocket::ConnectedState &&
               !m_heartbeatTimer.isActive()) {
        m_heartbeatTimer.start();
    }
}

void TcpClientDF::connectToServer(const QString &host, quint16 port)
{
    const QString normalizedHost = host.trimmed();
    if (normalizedHost.isEmpty() || port == 0) {
        qWarning() << "[LAN][RFSoC-TCP] connect rejected invalid target"
                   << normalizedHost << port;
        return;
    }

    m_userDisconnect = false;
    m_immediateRetryConsumed = false;
    m_reconnectCurrentMs = m_reconnectMs;

    if (m_reconnectTimer.isActive())
        m_reconnectTimer.stop();
    if (m_heartbeatTimer.isActive())
        m_heartbeatTimer.stop();

    // Target switches intentionally break the old connection. Suppress that
    // one disconnected callback from starting a retry against the old target.
    if (m_socket.state() == QAbstractSocket::ConnectedState)
        m_suppressNextDisconnected = true;

    if (m_socket.state() != QAbstractSocket::UnconnectedState)
        m_socket.abort();

    m_lastHost = normalizedHost;
    m_lastPort = port;
    m_buffer.clear();

    emit logMessage(QString("Connecting to %1:%2").arg(m_lastHost).arg(m_lastPort));
    qInfo().noquote() << "[LAN][RFSoC-TCP] connectToHost"
                      << QStringLiteral("%1:%2").arg(m_lastHost).arg(m_lastPort);

    m_socket.connectToHost(m_lastHost, m_lastPort);
    scheduleReconnect(QStringLiteral("initial-connect-watchdog"));
}

void TcpClientDF::disconnectFromServer()
{
    m_userDisconnect = true;
    m_immediateRetryConsumed = false;

    if (m_reconnectTimer.isActive())
        m_reconnectTimer.stop();
    if (m_heartbeatTimer.isActive())
        m_heartbeatTimer.stop();

    if (m_socket.state() != QAbstractSocket::UnconnectedState)
        m_socket.disconnectFromHost();
}

void TcpClientDF::onConnected()
{
    m_userDisconnect = false;
    m_immediateRetryConsumed = false;
    m_reconnectCurrentMs = m_reconnectMs;

    if (m_reconnectTimer.isActive())
        m_reconnectTimer.stop();

    emit logMessage("Connected to DoA server");
    qInfo().noquote() << "[LAN][RFSoC-TCP] connected"
                      << QStringLiteral("%1:%2").arg(m_lastHost).arg(m_lastPort);
    emit connected();

    if (m_heartbeatEnabled && !m_heartbeatTimer.isActive())
        m_heartbeatTimer.start();

    flushPendingWrites();
}

void TcpClientDF::onDisconnected()
{
    if (m_heartbeatTimer.isActive())
        m_heartbeatTimer.stop();

    if (m_suppressNextDisconnected) {
        m_suppressNextDisconnected = false;
        qInfo() << "[LAN][RFSoC-TCP] target-switch disconnect suppressed";
        return;
    }

    emit logMessage("Disconnected from DoA server");
    qWarning().noquote() << "[LAN][RFSoC-TCP] disconnected"
                         << QStringLiteral("%1:%2").arg(m_lastHost).arg(m_lastPort);
    emit disconnected();

    requestReconnect(QStringLiteral("disconnected"), true);
}

void TcpClientDF::onReadyRead()
{
    m_buffer.append(m_socket.readAll());

    while (true) {
        const int idx = m_buffer.indexOf('\n');
        if (idx < 0)
            break;

        const QByteArray line = m_buffer.left(idx);
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

    if (m_heartbeatTimer.isActive())
        m_heartbeatTimer.stop();

    requestReconnect(QStringLiteral("socket-error"), true);
}

void TcpClientDF::requestReconnect(const QString &reason, bool allowImmediate)
{
    if (m_userDisconnect || !hasTarget())
        return;

    if (m_socket.state() == QAbstractSocket::ConnectedState)
        return;

    scheduleReconnect(reason);

    if (allowImmediate && !m_immediateRetryConsumed) {
        m_immediateRetryConsumed = true;
        qInfo().noquote() << "[LAN][RFSoC-TCP] immediate reconnect queued"
                          << QStringLiteral("%1:%2").arg(m_lastHost).arg(m_lastPort)
                          << "reason=" << reason;
        QMetaObject::invokeMethod(this, "attemptReconnect", Qt::QueuedConnection);
    }
}

void TcpClientDF::scheduleReconnect(const QString &reason)
{
    if (m_userDisconnect || !hasTarget() || m_reconnectTimer.isActive())
        return;

    const int delayMs = qBound(m_reconnectMs,
                               m_reconnectCurrentMs,
                               m_reconnectMaxMs);
    m_reconnectTimer.start(delayMs);

    qInfo().noquote() << "[LAN][RFSoC-TCP] reconnect scheduled"
                      << QStringLiteral("%1:%2").arg(m_lastHost).arg(m_lastPort)
                      << "reason=" << reason
                      << "delay_ms=" << delayMs;

    const qint64 doubled = static_cast<qint64>(delayMs) * 2;
    m_reconnectCurrentMs = static_cast<int>(qMin<qint64>(m_reconnectMaxMs, doubled));
}

void TcpClientDF::attemptReconnect()
{
    if (m_userDisconnect || !hasTarget())
        return;

    const auto st = m_socket.state();
    if (st == QAbstractSocket::ConnectedState) {
        if (m_reconnectTimer.isActive())
            m_reconnectTimer.stop();
        if (m_heartbeatEnabled && !m_heartbeatTimer.isActive())
            m_heartbeatTimer.start();
        return;
    }

    if (st == QAbstractSocket::ConnectingState) {
        emit logMessage("Reconnect: connecting stuck -> abort & retry");
        qWarning().noquote() << "[LAN][RFSoC-TCP] connecting watchdog expired; retry"
                             << QStringLiteral("%1:%2").arg(m_lastHost).arg(m_lastPort);
        m_socket.abort();
    }

    emit logMessage(QString("Reconnecting to %1:%2 ...").arg(m_lastHost).arg(m_lastPort));
    qInfo().noquote() << "[LAN][RFSoC-TCP] reconnect attempt"
                      << QStringLiteral("%1:%2").arg(m_lastHost).arg(m_lastPort);

    m_socket.abort();
    m_socket.connectToHost(m_lastHost, m_lastPort);
    scheduleReconnect(QStringLiteral("retry-connect-watchdog"));
}

void TcpClientDF::sendHeartbeat()
{
    if (!m_heartbeatEnabled ||
        m_socket.state() != QAbstractSocket::ConnectedState)
        return;

    QJsonObject ping;
    ping["menuID"] = "ping";
    ping["ts"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QByteArray payload = QJsonDocument(ping).toJson(QJsonDocument::Compact);
    if (!payload.endsWith('\n'))
        payload.append('\n');

    const qint64 n = m_socket.write(payload);
    if (n < 0) {
        emit logMessage(QString("[Heartbeat] write failed: %1").arg(m_socket.errorString()));
        m_socket.abort();
        requestReconnect(QStringLiteral("heartbeat-write"), true);
    }
}

void TcpClientDF::processLine(const QByteArray &line)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
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
    const QJsonDocument doc(obj);
    const QByteArray line = doc.toJson(QJsonDocument::Compact);
    return sendLine(line, addNewline);
}

bool TcpClientDF::sendLine(const QByteArray &line, bool addNewline)
{
    QByteArray payload = line;
    if (addNewline && !payload.endsWith('\n'))
        payload.append('\n');

    if (m_socket.state() == QAbstractSocket::ConnectedState) {
        const qint64 n = m_socket.write(payload);
        if (n < 0) {
            emit logMessage(QString("write() failed: %1").arg(m_socket.errorString()));
            m_socket.abort();
            requestReconnect(QStringLiteral("write-failed"), true);
            return false;
        }
        return true;
    }

    if (m_pendingWrites.size() >= m_maxPending) {
        m_pendingWrites.pop_front();
        emit logMessage("Queue overflow - dropping 1 chunks");
    }

    m_pendingWrites.push_back(payload);
    requestReconnect(QStringLiteral("queued-write"), true);
    return false;
}

void TcpClientDF::flushPendingWrites()
{
    if (m_socket.state() != QAbstractSocket::ConnectedState)
        return;

    while (!m_pendingWrites.isEmpty()) {
        const QByteArray payload = m_pendingWrites.front();
        m_pendingWrites.pop_front();

        const qint64 n = m_socket.write(payload);
        if (n < 0) {
            emit logMessage(QString("flush write() failed: %1").arg(m_socket.errorString()));
            m_socket.abort();
            requestReconnect(QStringLiteral("flush-write-failed"), true);
            break;
        }
    }
}
