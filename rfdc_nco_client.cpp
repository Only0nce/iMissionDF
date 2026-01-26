#include "rfdc_nco_client.h"

#include <QHostAddress>
#include <QDebug>

RfdcNcoClient::RfdcNcoClient(QObject *parent)
    : QObject(parent),
    m_host(QStringLiteral("192.168.10.8")),  // เปลี่ยน IP ตาม RFSoC ของคุณ
    m_port(6000)
{
    connect(&m_socket, &QTcpSocket::connected,
            this, &RfdcNcoClient::onConnected);
    connect(&m_socket, &QTcpSocket::disconnected,
            this, &RfdcNcoClient::onDisconnected);
    connect(&m_socket, &QTcpSocket::readyRead,
            this, &RfdcNcoClient::onReadyRead);
    connect(&m_socket,
            QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &RfdcNcoClient::onSocketError);

    // ====== Auto reconnect timer ======
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout,
            this, &RfdcNcoClient::onReconnectTimeout);
}

/* ---------- properties ---------- */

void RfdcNcoClient::setHost(const QString &host)
{
    if (m_host == host)
        return;
    m_host = host;
    emit hostChanged();
}

void RfdcNcoClient::setPort(quint16 port)
{
    if (m_port == port)
        return;
    m_port = port;
    emit portChanged();
}

void RfdcNcoClient::setAutoReconnect(bool enable)
{
    if (m_autoReconnect == enable)
        return;

    m_autoReconnect = enable;
    emit autoReconnectChanged();

    if (!m_autoReconnect) {
        // ปิด auto reconnect ก็หยุด timer ด้วย
        m_reconnectTimer.stop();
    }
}

void RfdcNcoClient::setReconnectIntervalMs(int ms)
{
    if (ms < 100)
        ms = 100;  // กันค่าที่สั้นเกินไป

    if (m_reconnectIntervalMs == ms)
        return;

    m_reconnectIntervalMs = ms;
    emit reconnectIntervalMsChanged();
}

/* ---------- public API ---------- */

void RfdcNcoClient::connectToServer()
{
    // user เรียก connect เอง -> ไม่ถือว่าเป็น manualClose
    m_manualClose = false;

    if (m_socket.state() == QAbstractSocket::ConnectedState ||
        m_socket.state() == QAbstractSocket::ConnectingState)
        return;

    // ถ้ากำลังจะต่อใหม่ให้หยุด timer เดิมก่อน
    if (m_reconnectTimer.isActive())
        m_reconnectTimer.stop();

    emit logMessage(QStringLiteral("[NCO] Connecting to %1:%2")
                        .arg(m_host).arg(m_port));

    m_socket.connectToHost(m_host, m_port);
}

void RfdcNcoClient::disconnectFromServer()
{
    // user กด disconnect เอง -> ไม่ต้อง auto-reconnect
    m_manualClose = true;
    m_reconnectTimer.stop();

    if (m_socket.state() == QAbstractSocket::ConnectedState ||
        m_socket.state() == QAbstractSocket::ConnectingState)
    {
        emit logMessage(QStringLiteral("[NCO] Disconnect from host (manual)"));
        m_socket.disconnectFromHost();
    }
}

void RfdcNcoClient::setFrequency(double freqHz)
{
    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        emit errorOccurred(QStringLiteral("Socket not connected"));
        return;
    }

    // ตัวอย่าง protocol:  SETFREQ 144500000\n
    QString line = QStringLiteral("SETFREQ %1").arg(freqHz, 0, 'f', 0);
    sendLine(line);
}

void RfdcNcoClient::requestCurrentFrequency()
{
    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        emit errorOccurred(QStringLiteral("Socket not connected"));
        return;
    }

    sendLine(QStringLiteral("GETFREQ"));
}

/* ---------------- private helpers ---------------- */

void RfdcNcoClient::sendLine(const QString &line)
{
    QByteArray data = line.toUtf8();
    data.append('\n');

    qint64 n = m_socket.write(data);
    if (n == -1) {
        emit errorOccurred(QStringLiteral("write() failed: %1")
                               .arg(m_socket.errorString()));
    } else {
        emit logMessage(QStringLiteral("TX: %1")
                            .arg(QString::fromUtf8(data).trimmed()));
    }
}

/* ---------------- slots ---------------- */

void RfdcNcoClient::onConnected()
{
    emit logMessage(QStringLiteral("[NCO] Connected"));
    emit connectedChanged(true);

    // ต่อสำเร็จแล้วก็ไม่ต้อง reconnect อีก
    m_reconnectTimer.stop();
}

void RfdcNcoClient::onDisconnected()
{
    emit logMessage(QStringLiteral("[NCO] Disconnected"));
    emit connectedChanged(false);

    // ถ้าไม่ได้ disconnect จาก user เอง -> เตรียม reconnect
    if (!m_manualClose) {
        scheduleReconnect(QStringLiteral("disconnected"));
    }
}

void RfdcNcoClient::onReadyRead()
{
    m_rxBuffer.append(m_socket.readAll());

    // อ่านแบบ line-based
    int idx;
    while ((idx = m_rxBuffer.indexOf('\n')) != -1) {
        QByteArray line = m_rxBuffer.left(idx);
        m_rxBuffer.remove(0, idx + 1);

        QString s = QString::fromUtf8(line).trimmed();
        if (!s.isEmpty()) {
            emit logMessage(QStringLiteral("RX: %1").arg(s));
            processLine(s);
        }
    }
}

void RfdcNcoClient::onSocketError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    emit errorOccurred(m_socket.errorString());
    emit logMessage(QStringLiteral("[NCO] Socket error: %1")
                        .arg(m_socket.errorString()));

    // error จาก network -> ลอง reconnect ถ้าเปิด auto reconnect และไม่ใช่ manualClose
    if (!m_manualClose) {
        scheduleReconnect(QStringLiteral("socket error"));
    }
}

/* ---------------- protocol parser ---------------- */

void RfdcNcoClient::processLine(const QString &line)
{
    // รูปแบบตอบกลับจาก server:
    //   "OK"
    //   "FREQ 144500000"
    //   "ERR xxx"
    //   "BYE"

    if (line.startsWith(QStringLiteral("FREQ "))) {
        QString val = line.mid(5).trimmed();
        bool ok = false;
        double f = val.toDouble(&ok);
        if (ok) {
            emit frequencyUpdated(f);
        }
    } else if (line.startsWith(QStringLiteral("ERR"))) {
        emit errorOccurred(line);
    }
    // "OK" / "BYE" ไม่ต้องทำอะไรก็ได้ ถ้าอยาก handle เพิ่มก็ทำได้
}

/* --------- auto reconnect helpers ---------- */

void RfdcNcoClient::scheduleReconnect(const QString &reason)
{
    if (!m_autoReconnect)
        return;

    // ถ้าอยู่ใน state ที่กำลังต่อ/ต่ออยู่แล้วก็ไม่ต้องทำอะไร
    if (m_socket.state() == QAbstractSocket::ConnectedState ||
        m_socket.state() == QAbstractSocket::ConnectingState)
        return;

    if (!m_reconnectTimer.isActive()) {
        emit logMessage(QStringLiteral("[NCO] Auto reconnect in %1 ms (%2)")
                            .arg(m_reconnectIntervalMs)
                            .arg(reason));
        m_reconnectTimer.start(m_reconnectIntervalMs);
    }
}

void RfdcNcoClient::onReconnectTimeout()
{
    if (m_socket.state() == QAbstractSocket::UnconnectedState) {
        emit logMessage(QStringLiteral("[NCO] Auto reconnect..."));
        connectToServer();
    }
}
