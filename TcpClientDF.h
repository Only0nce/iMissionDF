// =============================== TcpClientDF.h ===============================
#pragma once

#include <QObject>
#include <QTimer>
#include <QTcpSocket>
#include <QQueue>
#include <QByteArray>
#include <QString>
#include <QJsonObject>

class TcpClientDF : public QObject
{
    Q_OBJECT
public:
    explicit TcpClientDF(QObject *parent = nullptr);
    ~TcpClientDF() override;

    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();

    // Optional: allow changing intervals at runtime
    void setReconnectIntervalMs(int ms);
    void setHeartbeatIntervalMs(int ms);

    // Optional: enable/disable heartbeat
    void setHeartbeatEnabled(bool en);

    // Send to DoA server. The legacy sendLine()/sendJson() path may queue
    // commands while the socket is offline. Remote LAN IP configuration must
    // never be deferred, so sendLineIfConnected() is the strict TCP-only path
    // used by LAN3/end0 and LAN4/end1.
    bool sendJson(const QJsonObject &obj, bool addNewline = true);
    bool sendLine(const QByteArray &line, bool addNewline = true);
    bool sendLineIfConnected(const QByteArray &line, bool addNewline = true);

    // R-LAN4A: read-only state used by the Network Settings page for the
    // external RFSoC LAN control channel. This is the TCP control-link state,
    // not the physical end0/end1 carrier state on the remote RFSoC.
    bool isConnected() const { return m_socket.state() == QAbstractSocket::ConnectedState; }
    QString targetHost() const { return m_lastHost; }
    quint16 targetPort() const { return m_lastPort; }
    bool hasTarget() const { return !m_lastHost.trimmed().isEmpty() && m_lastPort != 0; }
    int pendingWriteCount() const { return m_pendingWrites.size(); }

signals:
    void logMessage(const QString &msg);
    void connected();
    void disconnected();
    void errorOccurred(const QString &err);

    void doaResultReceived(const QJsonObject &obj);
    void updateFromTcpServer(const QJsonObject &obj);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError socketError);

    void attemptReconnect();
    void sendHeartbeat();

private:
    void processLine(const QByteArray &line);
    void updateFromJson(const QJsonObject &obj);
    void flushPendingWrites();
    void scheduleReconnect(const QString &reason);
    void requestReconnect(const QString &reason, bool allowImmediate);

    // --- sockets ---
    QTcpSocket m_socket;

    // --- timers ---
    QTimer m_reconnectTimer;
    QTimer m_heartbeatTimer;

    int  m_reconnectMs  = 10000;   // periodic retry/watchdog interval
    int  m_heartbeatMs  = 10000;   // 10s
    // The RFSoC control protocol has no negotiated heartbeat packet contract.
    // Do not inject synthetic {menuID:"ping"} traffic by default; connection
    // state is driven by QTcpSocket connected/disconnected/error events.
    bool m_heartbeatEnabled = false;
    bool m_userDisconnect = false;
    bool m_shuttingDown = false;
    // One immediate retry is allowed per outage; after that the proven
    // periodic watchdog retries at m_reconnectMs without a tight loop.
    bool m_immediateRetryConsumed = false;
    bool m_rxSeenThisSession = false;

    // --- last target ---
    QString m_lastHost;
    quint16 m_lastPort = 0;

    // --- rx buffer ---
    QByteArray m_buffer;

    // --- outgoing queue (when not connected) ---
    QQueue<QByteArray> m_pendingWrites;
    int m_maxPending = 200;
};
