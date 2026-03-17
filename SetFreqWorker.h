#ifndef SETFREQWORKER_H
#define SETFREQWORKER_H

#include <QObject>
#include <QTcpSocket>
#include <QHostAddress>

class SetFreqWorker : public QObject
{
    Q_OBJECT
public:
    explicit SetFreqWorker(QObject *parent = nullptr);
    ~SetFreqWorker() override;

public slots:
    void setHostPort(const QString &host, quint16 port);
    void requestSetFreq(quint64 freqHz, int timeoutMs);

signals:
    void setFreqDone(quint64 freqHz, bool ok);

private:
    QString m_host = "127.0.0.1";
    quint16 m_port = 6000;

    // ใช้ socket แบบ local ในฟังก์ชันเพื่อความปลอดภัย (thread worker)
    bool sendSetFreqOnce(quint64 freqHz, int timeoutMs);
};

#endif // SETFREQWORKER_H
