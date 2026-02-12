#include "TcpServerDF.h"
#include <QDebug>

TcpServerDF::TcpServerDF(quint16 listenPort, QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_listenPort(listenPort)
{
    // เมื่อมีการ connect ใหม่เข้ามา
    connect(m_server, &QTcpServer::newConnection,
            this, &TcpServerDF::onNewConnection);

    if (!m_server->listen(QHostAddress::Any, m_listenPort)) {
        qCritical() << "[TcpServerDF] Cannot listen on port"
                    << m_listenPort << ":" << m_server->errorString();
    } else {
        qInfo() << "[TcpServerDF] Listening on port" << m_listenPort;
    }
}

TcpServerDF::~TcpServerDF()
{
    m_server->close();

    for (QTcpSocket *socket : m_clients) {
        if (socket) {
            socket->disconnectFromHost();
            socket->deleteLater();
        }
    }
    m_clients.clear();
    m_pendingBuffers.clear();
}

void TcpServerDF::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        if (!socket)
            continue;

        m_clients << socket;
        m_pendingBuffers.insert(socket, QByteArray());

        const QHostAddress addr = socket->peerAddress();
        const quint16      port = socket->peerPort();

        qInfo() << "[TcpServerDF] New client from"
                << addr.toString() << ":" << port;

        emit clientConnected(addr, port);

        connect(socket, &QTcpSocket::readyRead,
                this,   &TcpServerDF::onReadyRead);

        connect(socket, &QTcpSocket::disconnected,
                this,   &TcpServerDF::onClientDisconnected);
    }
}

void TcpServerDF::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    if (!m_pendingBuffers.contains(socket)) {
        // safety
        m_pendingBuffers.insert(socket, QByteArray());
    }

    QByteArray &buffer = m_pendingBuffers[socket];
    buffer.append(socket->readAll());

    int index;
    while ((index = buffer.indexOf('\n')) != -1) {
        QByteArray line = buffer.left(index);
        buffer.remove(0, index + 1);

        QString message = QString::fromUtf8(line).trimmed();

        if (!message.isEmpty()) {
            const QHostAddress addr = socket->peerAddress();
            const quint16      port = socket->peerPort();

            // qDebug() << "[TcpServerDF] Received from"
            //          << addr.toString() << ":" << port
            //          << "msg =" << message;

            emit messageReceived(message, addr, port);
        }
    }
}

void TcpServerDF::onClientDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    const QHostAddress addr = socket->peerAddress();
    const quint16      port = socket->peerPort();

    qInfo() << "[TcpServerDF] Client disconnected"
            << addr.toString() << ":" << port;

    emit clientDisconnected(addr, port);

    m_clients.removeAll(socket);
    m_pendingBuffers.remove(socket);

    socket->deleteLater();
}

void TcpServerDF::broadcastLine(const QString &line)
{
    const QByteArray data = (line + "\n").toUtf8();

    for (QTcpSocket *socket : m_clients) {
        if (!socket)
            continue;
        if (socket->state() == QAbstractSocket::ConnectedState) {
            socket->write(data);
        }
    }
}

void TcpServerDF::sendLineTo(QTcpSocket *socket, const QString &line)
{
    if (!socket)
        return;
    if (socket->state() != QAbstractSocket::ConnectedState)
        return;

    socket->write((line + "\n").toUtf8());
}
