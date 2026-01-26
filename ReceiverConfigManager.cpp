#include "ReceiverConfigManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

void ReceiverConfigManager::loadPresetsFromFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open file:" << filePath;
        return;
    }

    QByteArray jsonData = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);

    if (!doc.isObject()) {
        qWarning() << "Invalid JSON format.";
        return;
    }

    QJsonObject rootObj = doc.object();
    loadFromJson(rootObj); // already implemented earlier
}

QVariantList ReceiverConfigManager::getPresetsAsList() const {
    QVariantList list;
    for (const QString &key : m_presetOrder) {
        QJsonObject obj = m_presets.value(key);
        obj.insert("profileId", key);
        list.append(obj.toVariantMap());
    }
    return list;
}


ReceiverConfigManager::ReceiverConfigManager(QObject *parent)
    : QObject(parent) {}

QJsonObject ReceiverConfigManager::getCurrentConfig() const {
    return m_currentConfig;
}

QJsonObject ReceiverConfigManager::getPreset(const QString &uuid) const {
    return m_presets.value(uuid, QJsonObject());
}

QStringList ReceiverConfigManager::getPresetUUIDs() const {
    return m_presets.keys();
}

void ReceiverConfigManager::addOrModifyPreset(const QString &uuid, const QJsonObject &config) {
    if (!m_presets.contains(uuid)) {
        m_presetOrder.append(uuid); // new entry, track order
    }
    m_presets[uuid] = config;
    emit presetsChanged();
}

void ReceiverConfigManager::deletePreset(const QString &uuid) {
    if (m_presets.remove(uuid) > 0) {
        m_presetOrder.removeAll(uuid);
        emit presetsChanged();
    }
}


void ReceiverConfigManager::deleteAllPresets() {
    if (!m_presets.isEmpty()) {
        m_presets.clear();
        m_presetOrder.clear();
        emit presetsChanged();
    }
}

void ReceiverConfigManager::setCurrentConfig(const QJsonObject &config) {
    m_currentConfig = config;
    emit currentConfigChanged(m_currentConfig);
}

void ReceiverConfigManager::loadFromJson(const QJsonObject &json) {
    m_presets.clear();
    m_presetOrder.clear();

    if (json.contains("presetsList") && json["presetsList"].isObject()) {
        QJsonObject presets = json["presetsList"].toObject();
        for (const QString &key : presets.keys()) {
            m_presets[key] = presets[key].toObject();
            m_presetOrder.append(key); // preserves file order
        }
    }

    if (json.contains("currentRx") && json["currentRx"].isObject()) {
        m_currentConfig = json["currentRx"].toObject();
    }

    emit presetsChanged();
    emit currentConfigChanged(m_currentConfig);
}


QJsonObject ReceiverConfigManager::saveToJson() const {
    QJsonObject root;
    QJsonObject presetsObj;
    for (const QString &key : m_presets.keys()) {
        presetsObj.insert(key, m_presets[key]);
    }
    root["presetsList"] = presetsObj;
    root["currentRx"] = m_currentConfig;
    return root;
}

bool ReceiverConfigManager::saveToFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Failed to open file for writing:" << filePath;
        return false;
    }

    QJsonDocument doc(saveToJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

void ReceiverConfigManager::updateRecorderConfig(const QString &alsaDev,
                                                 const QString &clientIp,
                                                 double clientFreq,
                                                 const QString &rtspIp,
                                                 const QString &rtspUri,
                                                 int rtspPort)
{
    QString filePath = "/var/lib/openwebrx/preset.json";
    QFile file(filePath);
    QJsonObject config;

    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to open config for reading:" << filePath;
            return;
        }
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
        file.close();

        if (parseError.error != QJsonParseError::NoError) {
            qWarning() << "JSON parse error:" << parseError.errorString();
        } else {
            config = doc.object();
        }
    }

    config["alsa_dev"] = alsaDev;
    config["client_as_ip"] = clientIp;
    config["client_as_freq"] = clientFreq;
    config["rtsp_server_ip"] = rtspIp;
    config["rtsp_server_uri"] = rtspUri;
    config["rtsp_server_port"] = rtspPort;

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Failed to open config for writing:" << filePath;
        return;
    }

    QJsonDocument newDoc(config);
    file.write(newDoc.toJson(QJsonDocument::Indented));
    file.close();

    qDebug() << "Recorder config updated successfully.";
}
