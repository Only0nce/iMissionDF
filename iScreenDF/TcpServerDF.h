#ifndef TCPSERVERDF_H
#define TCPSERVERDF_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QHash>

class TcpServerDF : public QObject
{
    Q_OBJECT
public:
    explicit TcpServerDF(quint16 listenPort,
                         QObject *parent = nullptr);
    ~TcpServerDF();

    bool isListening() const { return m_server->isListening(); }
    quint16 port() const { return m_listenPort; }

signals:

    void clientConnected(const QHostAddress &addr, quint16 port);

    void clientDisconnected(const QHostAddress &addr, quint16 port);
    void messageReceived(const QString &message,
                         const QHostAddress &addr,
                         quint16 port);

public slots:
    void broadcastLine(const QString &line);
    void sendLineTo(QTcpSocket *socket, const QString &line);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();

private:
    QTcpServer *m_server;
    quint16 m_listenPort;
    QList<QTcpSocket*> m_clients;

    QHash<QTcpSocket*, QByteArray> m_pendingBuffers;
};

#endif // TCPSERVERDF_H
