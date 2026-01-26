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

    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();

    // Optional: allow changing intervals at runtime
    void setReconnectIntervalMs(int ms);
    void setHeartbeatIntervalMs(int ms);

    // Optional: enable/disable heartbeat
    void setHeartbeatEnabled(bool en);

    // Send to DoA server
    bool sendJson(const QJsonObject &obj, bool addNewline = true);
    bool sendLine(const QByteArray &line, bool addNewline = true);

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

    // --- sockets ---
    QTcpSocket m_socket;

    // --- timers ---
    QTimer m_reconnectTimer;
    QTimer m_heartbeatTimer;

    int  m_reconnectMs  = 10000;   // 10s
    int  m_heartbeatMs  = 10000;   // 10s
    bool m_heartbeatEnabled = true;

    // --- last target ---
    QString m_lastHost;
    quint16 m_lastPort = 0;

    // --- rx buffer ---
    QByteArray m_buffer;

    // --- outgoing queue (when not connected) ---
    QQueue<QByteArray> m_pendingWrites;
    int m_maxPending = 200;
};
