#include "OpenWebRxConfig.h"

OpenWebRxConfig::OpenWebRxConfig(QObject *parent) : QObject(parent) {}

bool OpenWebRxConfig::loadFromFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError) return false;

    rootObject = doc.object();
    return true;
}

bool OpenWebRxConfig::saveToFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;

    QJsonDocument doc(rootObject);
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

QJsonObject OpenWebRxConfig::getProfiles() const {
    return rootObject["sdrs"].toObject()[sdrId].toObject()["profiles"].toObject();
}

QJsonObject OpenWebRxConfig::getProfile(const QString &uuid) const {
    return getProfiles().value(uuid).toObject();
}

bool OpenWebRxConfig::addOrUpdateProfile(const QString &uuid, const QJsonObject &profile) {
    auto sdrs = rootObject["sdrs"].toObject();
    auto xtrx = sdrs[sdrId].toObject();
    auto profiles = xtrx["profiles"].toObject();

    profiles[uuid] = profile;
    xtrx["profiles"] = profiles;
    sdrs[sdrId] = xtrx;
    rootObject["sdrs"] = sdrs;
    return true;
}

bool OpenWebRxConfig::removeProfile(const QString &uuid) {
    auto sdrs = rootObject["sdrs"].toObject();
    auto xtrx = sdrs[sdrId].toObject();
    auto profiles = xtrx["profiles"].toObject();

    if (!profiles.contains(uuid))
        return false;

    profiles.remove(uuid);
    xtrx["profiles"] = profiles;
    sdrs[sdrId] = xtrx;
    rootObject["sdrs"] = sdrs;
    return true;
}

QJsonObject OpenWebRxConfig::generateProfileListMessage() const
{
    QJsonArray profilesArray;
    QJsonObject profiles = getProfiles();

    for (const QString &profileId : profiles.keys()) {
        QString name = profiles.value(profileId).toObject().value("name").toString();
        QJsonObject profileItem;
        profileItem["id"] = sdrId + "|" + profileId;
        profileItem["name"] = "XTRX " + name;
        profilesArray.append(profileItem);
    }

    QJsonObject result;
    result["type"] = "updateProfiles";
    result["value"] = profilesArray;
    return result;
}
