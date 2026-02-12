#pragma once
#include <QObject>
#include <QThread>
#include <QTcpSocket>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>

class WorkerScan : public QObject {
    Q_OBJECT
public:
    explicit WorkerScan(QObject *parent = nullptr) : QObject(parent) {}

    QString baseIp;
    QString selfIp;
    int start = 1;
    int end = 254;
    int port = 9000;
    int timeoutMs = 200;

public slots:
    void process() {
        for (int i = start; i <= end; ++i) {
            QString ip = baseIp + QString::number(i);

            if (ip == selfIp)
                continue;

            QElapsedTimer timer;
            timer.start();

            QTcpSocket sock;
            sock.connectToHost(ip, port);

            if (!sock.waitForConnected(timeoutMs)) {
                continue;
            }

            int ping = timer.elapsed();

            // ขอ getName จาก TCP server
            sock.write("{\"menuID\":\"getName\"}\n");
            sock.flush();
            sock.waitForBytesWritten(50);

            QByteArray resp;

            // รอข้อมูลตอบกลับ
            if (sock.waitForReadyRead(150)) {
                resp += sock.readAll();
            }

            QString name, serial;

            if (!resp.isEmpty()) {

                QList<QByteArray> lines = resp.split('\n');
                for (const QByteArray &lineRaw : lines) {
                    QByteArray line = lineRaw.trimmed();
                    if (line.isEmpty())
                        continue;

                    QJsonParseError perr{};
                    QJsonDocument jdoc = QJsonDocument::fromJson(line, &perr);
                    if (perr.error != QJsonParseError::NoError || !jdoc.isObject())
                        continue;

                    QJsonObject o = jdoc.object();
                    QString menuID = o.value("menuID").toString();

                    if (menuID == "getNameReply" || menuID == "getName") {
                        name   = o.value("name").toString();
                        serial = o.value("serial").toString();

                        if (name.isEmpty())
                            name = o.value("Device").toString();
                        if (name.isEmpty())
                            name = o.value("hostname").toString();

                        break;
                    }
                }
            }

            sock.disconnectFromHost();

            if (!name.isEmpty()) {
                emit deviceFound(name, serial, ip, ping);
            }

            QThread::msleep(5);
        }

        emit scanFinished();
    }
signals:
    void scanFinished();
    void deviceFound(QString name, QString serial, QString ip, int ping);
};
