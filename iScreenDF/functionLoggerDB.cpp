#include "iScreenDF.h"

void iScreenDF::updateServerlogDB(const QString &ip){
    logdb = new DataloggerDB("RadioDF","orinnx","Orinnx!2025",ip);
    connect(logdb, &DataloggerDB::getdataDBLogReady, this, &iScreenDF::getdataDBLogReady);

    qDebug() << "remote Mysql :" << ip;
    // logdb->getDatalogServer();
}

void iScreenDF::requestDataLog() {
    logdb->getDatalogServer();
}

void iScreenDF::getdataDBLogReady(const QJsonArray &data){
    // qDebug() << "SandtoQML:" << data;
    // emit dataLogReady(data);
}
