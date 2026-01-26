#ifndef OPENWEBRXCONFIG_H
#define OPENWEBRXCONFIG_H

#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QMap>

class OpenWebRxConfig : public QObject {
    Q_OBJECT

public:
    explicit OpenWebRxConfig(QObject *parent = nullptr);
    bool loadFromFile(const QString &path);
    bool saveToFile(const QString &path);

    QJsonObject getProfiles() const;
    bool addOrUpdateProfile(const QString &uuid, const QJsonObject &profile);
    bool removeProfile(const QString &uuid);
    QJsonObject getProfile(const QString &uuid) const;
    QJsonObject generateProfileListMessage() const;

private:
    QJsonObject rootObject;
    QString sdrId = "b31f3c8e-ae06-48e7-b38c-9aa770bfde0b";
};

#endif // OPENWEBRXCONFIG_H
