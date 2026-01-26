#ifndef RFDC_NCO_CLIENT_H
#define RFDC_NCO_CLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>

class RfdcNcoClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString host READ host WRITE setHost NOTIFY hostChanged)
    Q_PROPERTY(quint16 port READ port WRITE setPort NOTIFY portChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(bool autoReconnect READ autoReconnect WRITE setAutoReconnect NOTIFY autoReconnectChanged)
    Q_PROPERTY(int reconnectIntervalMs READ reconnectIntervalMs WRITE setReconnectIntervalMs NOTIFY reconnectIntervalMsChanged)

public:
    explicit RfdcNcoClient(QObject *parent = nullptr);

    QString host() const { return m_host; }
    void setHost(const QString &host);

    quint16 port() const { return m_port; }
    void setPort(quint16 port);

    bool isConnected() const {
        return (m_socket.state() == QAbstractSocket::ConnectedState);
    }

    bool autoReconnect() const { return m_autoReconnect; }
    void setAutoReconnect(bool enable);

    int reconnectIntervalMs() const { return m_reconnectIntervalMs; }
    void setReconnectIntervalMs(int ms);

    Q_INVOKABLE void connectToServer();
    Q_INVOKABLE void disconnectFromServer();
    Q_INVOKABLE void setFrequency(double freqHz);
    Q_INVOKABLE void requestCurrentFrequency();

signals:
    void hostChanged();
    void portChanged();
    void connectedChanged(bool connected);
    void autoReconnectChanged();
    void reconnectIntervalMsChanged();

    void logMessage(const QString &msg);
    void errorOccurred(const QString &err);
    void frequencyUpdated(double freqHz);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onReconnectTimeout();

private:
    void sendLine(const QString &line);
    void processLine(const QString &line);
    void scheduleReconnect(const QString &reason);

    QTcpSocket m_socket;
    QString    m_host;
    quint16    m_port;
    QByteArray m_rxBuffer;

    QTimer m_reconnectTimer;
    bool   m_autoReconnect      = true;   // เปิด auto-reconnect เป็น default
    int    m_reconnectIntervalMs = 3000;  // 3 วินาทีต่อครั้ง
    bool   m_manualClose        = false;  // กันกรณี user กด disconnect เอง
};

#endif // RFDC_NCO_CLIENT_H
