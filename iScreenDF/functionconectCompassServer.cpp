#include "iScreenDF.h"
#include "CompassClient.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QDebug>

void iScreenDF::connectCompassServer(const QString &ip, quint16 port)
{
    qDebug() << "[iScreenDF::connectCompassServer]"
             << "ip =" << ip << "port =" << port;

    if (ip.isEmpty() || port == 0) {
        qWarning() << "[connectCompassServer] invalid ip/port";
        return;
    }

    if (!m_compassClient) {
        m_compassClient = new CompassClient(this);

        connect(m_compassClient, &CompassClient::compassConnected,
                this, &iScreenDF::onCompassConnected);

        connect(m_compassClient, &CompassClient::compassDisconnected,
                this, &iScreenDF::onCompassDisconnected);

        connect(m_compassClient, &CompassClient::compassError,
                this, &iScreenDF::onCompassError);

        connect(m_compassClient, &CompassClient::headingUpdated,
                this, &iScreenDF::onCompassHeadingUpdated);

        connect(m_compassClient, &CompassClient::calibStatusChanged,
                this, &iScreenDF::calibStatusChanged);
    }
    m_compassClient->connectToHost(ip, port);
}

void iScreenDF::disconnectCompassServer()
{
    qDebug() << "[iScreenDF::disconnectCompassServer]";

    if (!m_compassClient)
        return;

    m_compassClient->disconnectFromHost();

    // m_compassClient->deleteLater();
    // m_compassClient = nullptr;
}

void iScreenDF::onCompassConnected()
{
    qDebug() << "[iScreenDF::onCompassConnected] Compass connected";

    if (chatServerDF) {
        QJsonObject obj;
        obj["menuID"]   = "CompassConnected";
        obj["datetime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        chatServerDF->broadcastMessage(
            QJsonDocument(obj).toJson(QJsonDocument::Compact));
    }
}
void iScreenDF::onCompassDisconnected()
{
    qDebug() << "[iScreenDF::onCompassDisconnected] Compass disconnected";

    if (chatServerDF) {
        QJsonObject obj;
        obj["menuID"]   = "ReadDirection";
        obj["datetime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        chatServerDF->broadcastMessage(
            QJsonDocument(obj).toJson(QJsonDocument::Compact));
    }
}
void iScreenDF::onCompassError(const QString &err)
{
    qWarning() << "[iScreenDF::onCompassError]" << err;

    if (chatServerDF) {
        QJsonObject obj;
        obj["menuID"]   = "CompassError";
        obj["error"]    = err;
        obj["datetime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        chatServerDF->broadcastMessage(
            QJsonDocument(obj).toJson(QJsonDocument::Compact));
    }
}
void iScreenDF::calibStatusChanged(const QString &mode,const QString &state,const QString &rotate,double progressDeg,bool done,const QString &instruction)
{
    // qDebug() << "[CompassClient:calibStatusChanged] >> " << "mode: " << mode << "state: " << state << "rotate: " << rotate << "progressDeg: " << progressDeg << "done: " << done << "instruction: " << instruction;
    emit updateStatusCompass(instruction);
}
static inline double norm360(double deg)
{
    deg = std::fmod(deg, 360.0);
    if (deg < 0) deg += 360.0;
    return deg;
}
void iScreenDF::onCompassHeadingUpdated(double heading)
{
    // qDebug() << "[CompassClient:onCompassHeadingUpdated] >> " << heading ;
    if (m_parameter.isEmpty() || !m_parameter.first()) {
        qWarning() << "[iScreenDF] applyRfsocParameterToServer: no parameter";
        return;
    }
    Parameter *p = m_parameter.first();

    const QString serial = this->Serialnumber;     // หรือแหล่งจริงของคุณ
    const QString name   = this->controllerName;
    double heading_offset = norm360(heading +  p->m_compass_offset);
    emit updateDegree(serial,name,heading_offset);
    QJsonObject obj;
    obj["menuID"]   = "Compass";
    obj["heading"]    = heading_offset;
    obj["datetime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    chatServerDF->broadcastMessage(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    broadcastMessageServerandClient(obj);
    emit updateDegreelocal(heading);
}

// void iScreenDF::onCompassHeadingUpdated(double heading)
// {
//     // qDebug() << "[CompassClient:onCompassHeadingUpdated] >> " << heading ;
//     emit updateDegree(heading);
//     QJsonObject obj;
//     obj["menuID"]   = "Compass";
//     obj["heading"]    = heading;
//     obj["datetime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
//     chatServerDF->broadcastMessage(QJsonDocument(obj).toJson(QJsonDocument::Compact));
// }

void iScreenDF::Calibration(const QString &std)
{
    // qDebug() << "[CompassClient:Calibration] >> " << "std: " << std ;
    m_compassClient->sendCalZeroCommand();
}
