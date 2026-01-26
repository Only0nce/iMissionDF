#ifndef STORAGEMANAGEMENT_H
#define STORAGEMANAGEMENT_H

#include <QObject>
#include <QDebug>
#include <QThread>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
class StorageManagement : public QObject
{
    Q_OBJECT
public:
    explicit StorageManagement(QObject *parent = nullptr);

signals:

public slots:
    void checkDiskAndFormat(QString);

private slots:

};

#endif // DATABASE_H
