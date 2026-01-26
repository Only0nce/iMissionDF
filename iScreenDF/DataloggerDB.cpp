#include "DataloggerDB.h"
DataloggerDB::DataloggerDB(const QString &dbName,const QString &user,const QString &password,const QString &host, QObject *parent) : QObject(parent) {
    QString connName = "logdb_connection";
    {
        if (QSqlDatabase::contains(connName)) {
            QSqlDatabase db = QSqlDatabase::database(connName);
            db.close();
        }
        QSqlDatabase::removeDatabase(connName);
    }

    logdb = QSqlDatabase::addDatabase("QMYSQL", connName);
    logdb.setHostName(host);
    logdb.setDatabaseName(dbName);
    logdb.setUserName(user);
    logdb.setPassword(password);
}

DataloggerDB::~DataloggerDB() {}

bool DataloggerDB::database_createConnection() {
    if (!logdb.open()) {
        qDebug() << "database error! database can not open.";
        // restartMysql();
        return false;
    }
    //    db.close();
    qDebug() << "Database connected";
    return true;
}

void DataloggerDB::getDatalogServer(){
    qDebug() << "getNetwork from database";
    if (!logdb.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!logdb.open()) {
            qDebug() << "Failed to reconnect database:" << logdb.lastError().text();
            return;
        }
    }
    QSqlQuery query(logdb);
    QString queryStr = QString("SELECT id, frequency, log_datetime, direction, mgrs, lat, lon, altitude, heading, pic FROM data_log ORDER BY log_datetime DESC;");
    if (!query.exec(queryStr)) {
        qDebug() << "Query failed:" << query.lastError().text();
        return;
    }
    QJsonArray logArray;
    while (query.next()) {
        QJsonObject obj;
        obj["id"] = query.value(0).toInt();
        obj["frequency"] = query.value(1).toDouble();
        QString logDatetime = query.value(2).toString().replace("T", " ");
        obj["log_datetime"] = logDatetime;
        obj["direction"] = query.value(3).toDouble();
        obj["mgrs"] = query.value(4).toString();
        obj["lat"] = query.value(5).toDouble();
        obj["lon"] = query.value(6).toDouble();
        obj["altitude"] = query.value(7).toDouble();
        obj["heading"] = query.value(8).toDouble();
        // QString originalPicPath = query.value(9).toString();
        // QString fileName = QFileInfo(originalPicPath).fileName(); // ได้ "500236.png"
        // QString newPath = "/var/www/html/image/" + fileName;
        obj["pic"] = query.value(9).toString();

        logArray.append(obj);
        qDebug() << "Query pic:" << obj;
    }
    emit getdataDBLogReady(logArray);
    logdb.close();
}

