#ifndef RECEIVERCONFIGMANAGER_H
#define RECEIVERCONFIGMANAGER_H

#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonArray>
#include <QMap>
#include <QUuid>

class ReceiverConfigManager : public QObject {
    Q_OBJECT

public:
    explicit ReceiverConfigManager(QObject *parent = nullptr);

    Q_INVOKABLE QJsonObject getCurrentConfig() const;
    Q_INVOKABLE QJsonObject getPreset(const QString &uuid) const;
    Q_INVOKABLE QStringList getPresetUUIDs() const;

    Q_INVOKABLE void addOrModifyPreset(const QString &uuid, const QJsonObject &config);
    Q_INVOKABLE void deletePreset(const QString &uuid);
    Q_INVOKABLE void deleteAllPresets();

    Q_INVOKABLE void setCurrentConfig(const QJsonObject &config);

    Q_INVOKABLE void loadPresetsFromFile(const QString &filePath);
    Q_INVOKABLE QVariantList getPresetsAsList() const;

    Q_INVOKABLE void updateRecorderConfig(const QString &alsaDev,
                                          const QString &clientIp,
                                          double clientFreq,
                                          const QString &rtspIp,
                                          const QString &rtspUri,
                                          int rtspPort);

    void loadFromJson(const QJsonObject &json);
    QJsonObject saveToJson() const;
    Q_INVOKABLE bool saveToFile(const QString &filePath);
signals:
    void currentConfigChanged(const QJsonObject &newConfig);
    void presetsChanged();

private:
    QMap<QString, QJsonObject> m_presets;
    QJsonObject m_currentConfig;
    QList<QString> m_presetOrder;  // Tracks insertion order
};


#endif // RECEIVERCONFIGMANAGER_H
