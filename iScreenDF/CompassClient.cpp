#include "CompassClient.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

CompassClient::CompassClient(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, &QTcpSocket::readyRead,
            this, &CompassClient::onReadyRead);

    connect(&m_socket, &QTcpSocket::connected,
            this, &CompassClient::onConnected);

    connect(&m_socket, &QTcpSocket::disconnected,
            this, &CompassClient::onDisconnected);

    connect(&m_socket,
            QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this,
            &CompassClient::onErrorOccurred);

    m_reconnectTimer.setInterval(10000);
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout,
            this, &CompassClient::onReconnectTimeout);
}

void CompassClient::connectToHost(const QString &host, quint16 port)
{
    m_host = host;
    m_port = port;
    m_userDisconnect = false;

    qDebug() << "[CompassClient] Connecting to" << host << ":" << port;
    m_socket.connectToHost(host, port);
}

void CompassClient::disconnectFromHost()
{
    qDebug() << "[CompassClient] disconnectFromHost()";
    m_userDisconnect = true;    // ไม่ให้ auto reconnect
    m_reconnectTimer.stop();

    if (m_socket.state() != QAbstractSocket::UnconnectedState)
        m_socket.disconnectFromHost();
}

// ------------------------------------------------------------------


void CompassClient::sendJsonCommand(const QString &jsonLine)
{
    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        qDebug() << "[CompassClient] sendJsonCommand but not connected";
        return;
    }

    QByteArray data = jsonLine.toUtf8();
    if (!data.endsWith('\n'))
        data.append('\n');  // server ฝั่ง C รอ \n ตัดบรรทัด

    qDebug() << "[CompassClient] >>>" << data;
    m_socket.write(data);
    m_socket.flush();
}

void CompassClient::sendCalZeroCommand()
{
    // {"command":"CAL_ZERO"}
    QJsonObject obj;
    obj["command"] = QStringLiteral("CAL_ZERO");
    QJsonDocument doc(obj);
    QString jsonLine = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    sendJsonCommand(jsonLine);
}

// ------------------------------------------------------------------
// slots
// ------------------------------------------------------------------

void CompassClient::onConnected()
{
    qDebug() << "[CompassClient] TCP connected!";
    emit compassConnected();
    // sendCalZeroCommand();
}

void CompassClient::onDisconnected()
{
    qDebug() << "[CompassClient] TCP disconnected";
    emit compassDisconnected();

    if (m_userDisconnect) {
        qDebug() << "[CompassClient] User disconnect → ไม่ reconnect";
        return;
    }

    // ถ้าหลุดเอง → รอ 10 วิ แล้วค่อย reconnect
    if (!m_reconnectTimer.isActive()) {
        qDebug() << "[CompassClient] Auto reconnect in 10 sec...";
        m_reconnectTimer.start();
    }
}

void CompassClient::onErrorOccurred(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    qDebug() << "[CompassClient] Socket error:" << m_socket.errorString();
    emit compassError(m_socket.errorString());

    if (m_userDisconnect)
        return;

    if (!m_reconnectTimer.isActive()) {
        qDebug() << "[CompassClient] Error → reconnect in 10 sec...";
        m_reconnectTimer.start();
    }
}

void CompassClient::onReconnectTimeout()
{
    if (m_userDisconnect)
        return;

    if (m_host.isEmpty() || m_port == 0)
        return;

    qDebug() << "[CompassClient] Reconnecting to" << m_host << ":" << m_port;
    m_socket.abort();
    m_socket.connectToHost(m_host, m_port);
}

void CompassClient::onReadyRead()
{
    m_buffer.append(QString::fromUtf8(m_socket.readAll()));

    int idx;
    while ((idx = m_buffer.indexOf('\n')) != -1) {
        QString line = m_buffer.left(idx).trimmed();
        m_buffer.remove(0, idx + 1);

        if (line.isEmpty())
            continue;

        // Parse JSON
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError) {
            qDebug() << "[CompassClient] JSON parse error:" << err.errorString()
            << "line:" << line;
            continue;
        }

        if (!doc.isObject()) {
            qDebug() << "[CompassClient] JSON not an object:" << line;
            continue;
        }

        QJsonObject obj = doc.object();

        QString objectName = obj.value("objectName").toString();
        // qDebug() << "[CompassClient] objectName:" << objectName << "raw obj:" << obj;
        if (objectName == "AngleOfHeading") {
            QJsonValue v = obj.value("value");

            if (v.isDouble()) {
                double heading = v.toDouble();
                emit headingUpdated(heading);
            } else {
                qDebug() << "[CompassClient] Invalid value number:" << line;
            }
        }
        if (obj.contains("calib") || obj.contains("guide")) {
            QJsonObject calib = obj.value("calib").toObject();
            QJsonObject guide = obj.value("guide").toObject();

            QString mode        = calib.value("mode").toString();
            QString state       = calib.value("state").toString();
            QString rotate      = guide.value("rotate").toString();
            double  progressDeg = guide.value("progress_deg").toDouble();
            bool    done        = guide.value("done").toBool();
            QString instruction = guide.value("instruction").toString();
            // qDebug() << "[CompassClient:calibration] >> " << "mode: " << mode << "state: " << state << "rotate: " << rotate << "progressDeg: " << progressDeg << "done: " << done << "instruction: " << instruction;
            emit calibStatusChanged(mode, state, rotate, progressDeg, done, instruction);
        }
    }
}

