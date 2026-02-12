#ifndef SETFREQWORKER_H
#define SETFREQWORKER_H

#include <QObject>
#include <QString>
#include <QProcess>
#include <QDebug>
#include <QThread>
#include <QTcpSocket>

class SetFreqWorker : public QObject
{
    Q_OBJECT
public:
    explicit SetFreqWorker(QObject *parent = nullptr) : QObject(parent) {}

public slots:
    void setHostPort(const QString &host, quint16 port) {
        m_host = host;
        m_port = port;
        qDebug() << "[SETFREQ][WORKER] setHostPort" << m_host << m_port;
    }

    void requestSetFreq(quint64 freqHz, int timeoutMs) {
        // ทำงานใน thread นี้
        const bool ok = doSetFreq(freqHz, timeoutMs);
        emit setFreqDone(freqHz, ok);
    }

signals:
    void setFreqDone(quint64 freqHz, bool ok);

private:
#include <QElapsedTimer>
#include <QThread>

    bool doSetFreq(quint64 freqHz, int timeoutMs)
    {
        QTcpSocket s;
        s.connectToHost(m_host, m_port);
        if (!s.waitForConnected(timeoutMs)) {
            qWarning() << "[SETFREQ][ERROR] connect:" << s.errorString();
            return false;
        }

        const QByteArray payload = QByteArray("SETFREQ ") + QByteArray::number(freqHz) + "\n";
        if (s.write(payload) != payload.size() || !s.waitForBytesWritten(timeoutMs)) {
            qWarning() << "[SETFREQ][ERROR] write:" << s.errorString();
            s.disconnectFromHost();
            return false;
        }

        // ถ้าต้องการอ่าน "OK"
        if (s.waitForReadyRead(300)) {
            const QByteArray reply = s.readAll();
            qWarning() << "[SETFREQ] reply =" << reply.trimmed();
        }

        s.disconnectFromHost();
        s.waitForDisconnected(300);
        return true;
        // QElapsedTimer tAll;
        // tAll.start();

        // auto ms = [&](qint64 v){ return QString::number(v) + "ms"; };

        // qWarning() << "[SETFREQ][T]" << QThread::currentThread()
        //            << "[STEP 0] enter"
        //            << "freqHz=" << freqHz
        //            << "timeoutMs=" << timeoutMs;

        // const QByteArray payload = QByteArray("SETFREQ ") + QByteArray::number(freqHz) + "\n";
        // qWarning() << "[SETFREQ][STEP 1] payload =" << payload.trimmed();
        // qWarning() << "[SETFREQ][STEP 1] host/port =" << m_host << m_port;

        // QProcess p;
        // p.setProgram("nc");
        // p.setArguments({ "-w", "1", m_host, QString::number(m_port) });
        // // p.setArguments({ m_host, QString::number(m_port) });
        // p.setProcessChannelMode(QProcess::MergedChannels);

        // qWarning() << "[SETFREQ][STEP 2] starting nc...";
        // QElapsedTimer tStart;
        // tStart.start();
        // p.start();

        // if (!p.waitForStarted(timeoutMs)) {
        //     qWarning() << "[SETFREQ][ERROR] waitForStarted timeout after" << ms(tStart.elapsed())
        //     << "state=" << p.state()
        //     << "err=" << p.errorString();
        //     qWarning() << "[SETFREQ][TOTAL] elapsed" << ms(tAll.elapsed());
        //     return false;
        // }
        // qWarning() << "[SETFREQ][STEP 3] started in" << ms(tStart.elapsed())
        //            << "pid=" << p.processId()
        //            << "state=" << p.state();

        // // --- write payload ---
        // qWarning() << "[SETFREQ][STEP 4] writing payload bytes=" << payload.size();
        // QElapsedTimer tWrite;
        // tWrite.start();

        // const qint64 written = p.write(payload);
        // p.closeWriteChannel();

        // // wait bytes written (ช่วยดูว่าค้างตรงส่งหรือเปล่า)
        // bool wroteOk = p.waitForBytesWritten(timeoutMs);

        // qWarning() << "[SETFREQ][STEP 4] write() returned=" << written
        //            << "waitForBytesWritten=" << wroteOk
        //            << "writeElapsed=" << ms(tWrite.elapsed())
        //            << "state=" << p.state();

        // // --- wait finish ---
        // qWarning() << "[SETFREQ][STEP 5] waiting for nc to finish...";
        // QElapsedTimer tFinish;
        // tFinish.start();

        // if (!p.waitForFinished(timeoutMs)) {
        //     const QByteArray partial = p.readAll(); // อ่านเท่าที่มีตอนนี้
        //     qWarning() << "[SETFREQ][ERROR] waitForFinished timeout after" << ms(tFinish.elapsed())
        //                << "state=" << p.state()
        //                << "partialOut=" << partial.trimmed();

        //     qWarning() << "[SETFREQ][ACTION] kill process";
        //     p.kill();
        //     p.waitForFinished(300);

        //     const QByteArray outAfterKill = p.readAll();
        //     qWarning() << "[SETFREQ][AFTER KILL] exitCode=" << p.exitCode()
        //                << "exitStatus=" << p.exitStatus()
        //                << "out=" << outAfterKill.trimmed();

        //     qWarning() << "[SETFREQ][TOTAL] elapsed" << ms(tAll.elapsed());
        //     return false;
        // }

        // const QByteArray output = p.readAll();
        // const int exitCode = p.exitCode();

        // qWarning() << "[SETFREQ][STEP 6] finished in" << ms(tFinish.elapsed())
        //            << "exitCode=" << exitCode
        //            << "exitStatus=" << p.exitStatus()
        //            << "output=" << output.trimmed();

        // if (p.exitStatus() != QProcess::NormalExit || exitCode != 0) {
        //     qWarning() << "[SETFREQ][ERROR] nc failed";
        //     qWarning() << "[SETFREQ][TOTAL] elapsed" << ms(tAll.elapsed());
        //     return false;
        // }

        // qWarning() << "[SETFREQ][OK] sent" << freqHz
        //            << "[TOTAL] elapsed" << ms(tAll.elapsed());
        // return true;
    }


private:
    QString m_host = "127.0.0.1";
    quint16 m_port = 6000;
};

#endif // SETFREQWORKER_H
