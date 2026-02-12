
#include "ReceiverRecorderConfigManager.h"
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>

ReceiverRecorderConfigManager::ReceiverRecorderConfigManager(QObject *parent)
    : QObject(parent)
{
}

void ReceiverRecorderConfigManager::updateRecorderConfig(const QString &alsaDev,
                                                         const QString &clientIp,
                                                         double clientFreq,
                                                         const QString &rtspIp,
                                                         const QString &rtspUri,
                                                         int rtspPort)
{
    QJsonObject config;
    config["alsa_dev"] = alsaDev;
    config["client_as_ip"] = clientIp;
    config["client_as_freq"] = clientFreq;
    config["rtsp_server_ip"] = rtspIp;
    config["rtsp_server_uri"] = rtspUri;
    config["rtsp_server_port"] = rtspPort;

    QFile file("/var/lib/openwebrx/recorder.json");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Failed to write recorder.json";
        return;
    }

    QJsonDocument doc(config);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qDebug() << "Recorder config saved.";
    emit onRecorderConfigSaved();
}

void ReceiverRecorderConfigManager::getConfig()
{
    QFile file("/var/lib/openwebrx/recorder.json");
    if (!file.exists()) {
        qWarning() << "recorder.json does not exist.";
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open recorder.json for reading.";
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error in recorder.json:" << parseError.errorString();
        return;
    }

    QJsonObject obj = doc.object();
    emit configLoaded(
        obj.value("alsa_dev").toString(),
        obj.value("client_as_ip").toString(),
        obj.value("client_as_freq").toDouble(),
        obj.value("rtsp_server_ip").toString(),
        obj.value("rtsp_server_uri").toString(),
        obj.value("rtsp_server_port").toInt()
    );
}

QVariantMap ReceiverRecorderConfigManager::loadConfig()
{
    QVariantMap result;
    QFile file("/var/lib/openwebrx/recorder.json");
    if (!file.exists()) {
        qWarning() << "recorder.json does not exist.";
        return result;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open recorder.json for reading.";
        return result;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error in recorder.json:" << parseError.errorString();
        return result;
    }

    QJsonObject obj = doc.object();
    for (const QString &key : obj.keys()) {
        result.insert(key, obj.value(key).toVariant());
    }

    return result;
}
