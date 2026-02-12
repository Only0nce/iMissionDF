#ifndef COMPASSCLIENT_H
#define COMPASSCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>

class CompassClient : public QObject
{
    Q_OBJECT
public:
    explicit CompassClient(QObject *parent = nullptr);

    Q_INVOKABLE void connectToHost(const QString &host, quint16 port);
    Q_INVOKABLE void disconnectFromHost();

    // ฟังก์ชันที่ไว้ส่ง command ไปหา server
    Q_INVOKABLE void sendCalZeroCommand();   // {"command":"CAL_ZERO"}
    Q_INVOKABLE void sendJsonCommand(const QString &jsonLine); // ส่ง JSON อื่น ๆ ตามต้องการ

signals:
    void headingUpdated(double heading);
    void compassConnected();
    void compassDisconnected();
    void compassError(const QString &error);
    void calibStatusChanged(const QString &mode,
                            const QString &state,
                            const QString &rotate,
                            double progressDeg,
                            bool done,
                            const QString &instruction);
private slots:
    void onConnected();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError socketError);
    void onReconnectTimeout();
    void onReadyRead();

private:
    QTcpSocket m_socket;
    QTimer     m_reconnectTimer;
    QString    m_host;
    quint16    m_port = 0;
    bool       m_userDisconnect = false;
    QString    m_buffer;
};

#endif // COMPASSCLIENT_H
