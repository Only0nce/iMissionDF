#include "SetFreqWorker.h"
#include <QDebug>

SetFreqWorker::SetFreqWorker(QObject *parent)
    : QObject(parent)
{
}

SetFreqWorker::~SetFreqWorker()
{
}

void SetFreqWorker::setHostPort(const QString &host, quint16 port)
{
    m_host = host.trimmed();
    m_port = port;
    qWarning() << "[SetFreqWorker] setHostPort host=" << m_host << "port=" << m_port;
}

void SetFreqWorker::requestSetFreq(quint64 freqHz, int timeoutMs)
{
    const bool ok = sendSetFreqOnce(freqHz, timeoutMs);
    emit setFreqDone(freqHz, ok);
}

bool SetFreqWorker::sendSetFreqOnce(quint64 freqHz, int timeoutMs)
{
    if (m_host.isEmpty() || m_port == 0) {
        qWarning() << "[SetFreqWorker] invalid host/port";
        return false;
    }

    QTcpSocket sock;
    sock.connectToHost(m_host, m_port);

    if (!sock.waitForConnected(timeoutMs)) {
        qWarning() << "[SetFreqWorker] connect failed:" << sock.errorString()
        << "host=" << m_host << "port=" << m_port;
        return false;
    }

    // โปรโตคอลของคุณ: "SETFREQ <Hz>\n"
    const QByteArray cmd = QByteArray("SETFREQ ") + QByteArray::number(freqHz) + "\n";
    const qint64 w = sock.write(cmd);

    if (w != cmd.size()) {
        qWarning() << "[SetFreqWorker] write failed size mismatch:" << w << "/" << cmd.size()
        << sock.errorString();
        sock.disconnectFromHost();
        return false;
    }

    if (!sock.waitForBytesWritten(timeoutMs)) {
        qWarning() << "[SetFreqWorker] waitForBytesWritten timeout"
                   << sock.errorString();
        sock.disconnectFromHost();
        return false;
    }

    // ถ้ามี response จาก server จะอ่านก็ได้ (optional)
    // sock.waitForReadyRead(50);
    // QByteArray resp = sock.readAll();
    // qDebug() << "[SetFreqWorker] resp:" << resp;

    sock.disconnectFromHost();
    sock.waitForDisconnected(200);

    return true;
}
