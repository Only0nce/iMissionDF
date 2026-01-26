#ifndef DATALOGGERDB_H
#define DATALOGGERDB_H

#include <QObject>
#include <QSqlDatabase>
#include <QtSql>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include "QWebSocketServer"
#include "QWebSocket"

class DataloggerDB : public QObject
{
    Q_OBJECT
public:
    explicit DataloggerDB(const QString &dbName, const QString &user, const QString &password, const QString &host, QObject *parent = nullptr);
    ~DataloggerDB();
    bool database_createConnection();
    void getDatalogServer();
private:
    QSqlDatabase logdb;

signals:
    void getdataDBLogReady(const QJsonArray &array);

};

#endif // DATALOGGERDB_H
