#include "Databases.h"



Database::Database(QString dbName, QString user, QString password, QString host, QObject *parent)
    : QObject(parent)
{
    // ✅ สร้างชื่อ connection ไม่ซ้ำกัน ต่อ instance
    // (ใช้ pointer ของ this + thread id กันชนข้าม thread)
    m_connName = QString("ScanRF_%1_%2")
                     .arg(reinterpret_cast<quintptr>(this))
                     .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));

    qDebug() << "ONLY Database name:" << dbName << "conn:" << m_connName;

    // ✅ ใส่ connection name เสมอ
    db = QSqlDatabase::addDatabase("QMYSQL", m_connName);
    db.setHostName(host);
    db.setDatabaseName(dbName);
    db.setUserName(user);
    db.setPassword(password);

    qDebug() << "Database name:" << dbName << "conn:" << db.connectionName();

    database_createConnection();
}

Database::~Database()
{
    // ✅ ปิดก่อน
    if (db.isValid() && db.isOpen()) {
        db.close();
    }

    const QString conn = m_connName;

    // ✅ ปล่อย handle ก่อน removeDatabase
    db = QSqlDatabase();

    // ✅ removeDatabase ต้องทำหลังไม่มี QSqlQuery ใช้งานแล้วเท่านั้น
    if (!conn.isEmpty() && QSqlDatabase::contains(conn)) {
        QSqlDatabase::removeDatabase(conn);
        qDebug() << "Removed DB connection:" << conn;
    }
}


void Database::restartMysql() {
    system("systemctl stop mysql");
    system("systemctl start mysql");

    qDebug() << "Restart MySQL";
}

bool Database::database_createConnection() {
    if (!db.open()) {
        qDebug() << "ONLY database error! database can not open.";
        restartMysql();
        return false;
    }
    db.close();
    qDebug() << "Database connected";
    return true;
}

void Database::getFrequency(){

}

void Database::updateFrequency(){

}

void Database::insertFrequency(){

}

void Database::insertScanCard(double freq,
                              const QString &unit,
                              const QString &bw,
                              const QString &mode,
                              int lowCut,
                              int highCut,
                              const QString &path,
                              QString time)
{
    if (!db.isOpen()) {
        if (!db.open()) {
            qDebug() << "[deleteScanCardById] Database cannot open!";
            restartMysql();
            return;
        }
    }

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO scan_cards
            (freq, unit, bw, mode, low_cut, high_cut, path, time)
        VALUES
            (:freq, :unit, :bw, :mode, :low_cut, :high_cut, :path, :time)
    )");

    query.bindValue(":freq",     freq);
    query.bindValue(":unit",     unit);
    query.bindValue(":bw",       bw);
    query.bindValue(":mode",     mode);
    query.bindValue(":low_cut",  lowCut);
    query.bindValue(":high_cut", highCut);
    query.bindValue(":path",     path);
    query.bindValue(":time", time);

    if (!query.exec()) {
        qWarning() << "[insertScanCard] SQL error:"
                   << query.lastError().text();
        qWarning() << "  Last query:" << query.lastQuery();
        return;
    }
    db.close();
    qDebug() << "[insertScanCard] Insert OK, id =" << query.lastInsertId();
}

void Database::deleteScanCardAll()
{
    if (!db.isOpen()) {
        if (!db.open()) {
            qWarning() << "[deleteScanCardAll] DB open failed";
            restartMysql();
            return;
        }
    }

    QSqlQuery query(db);
    if (!query.exec("TRUNCATE TABLE scan_cards")) {
        qWarning() << "[deleteScanCardAll] SQL error:"
                   << query.lastError().text();
        return;
    }

    db.close();
    qDebug() << "[deleteScanCardAll] Table truncated (all rows removed + auto_increment reset).";
}

void Database::deleteScanCardGroup(const QString &groupDateTime)
{
    if (!db.isOpen()) {
        if (!db.open()) {
            qWarning() << "[deleteScanCardGroup] DB open failed";
            restartMysql();
            return;
        }
    }

    // groupDateTime ต้องเป็น format "2025-12-04 15:25:39"
    QSqlQuery query(db);
    query.prepare(R"(
        DELETE FROM scan_cards
        WHERE time = :time
    )");
    query.bindValue(":time", groupDateTime);

    if (!query.exec()) {
        qWarning() << "[deleteScanCardGroup] SQL error:"
                   << query.lastError().text();
        return;
    }

    int rows = query.numRowsAffected();

    qInfo() << "[deleteScanCardGroup] Deleted" << rows
            << "rows for group" << groupDateTime;
}

void Database::deleteScanCardById(int id)
{
    if (!db.isOpen()) {
        if (!db.open()) {
            qDebug() << "[deleteScanCardById] Database cannot open!";
            restartMysql();
            return;
        }
    }

    QSqlQuery query(db);
    query.prepare(R"(
        DELETE FROM scan_cards
        WHERE id = :id
    )");

    query.bindValue(":id", id);

    if (!query.exec()) {
        qWarning() << "[deleteScanCardById] SQL error:"
                   << query.lastError().text();
        qWarning() << "  Last query:" << query.lastQuery();
        db.close();
        return;
    }

    db.close();
    qDebug() << "[deleteScanCardById] Deleted id =" << id;
}

void Database::getAllScanCards()
{
    qDebug() << "[getAllScanCards] database call.";
    QVector<ScanCard> result;
    QJsonArray jsonArray;

    if (!db.isOpen()) {
        if (!db.open()) {
            qDebug() << "[getAllScanCards] database error! database can not open.";
            restartMysql();
            return;
        }
    }

    QSqlQuery query(db);
    if (!query.exec(R"(
        SELECT id, freq, unit, bw, mode, low_cut, high_cut, path, time
        FROM scan_cards
        ORDER BY id ASC
    )")) {
        qWarning() << "[getAllScanCards] SQL error:"
                   << query.lastError().text();
        qWarning() << "  Last query:" << query.lastQuery();
        db.close();
        return;
    }

    while (query.next()) {
        // ==== Fill struct ====
        ScanCard row;
        row.id        = query.value("id").toInt();
        row.freq      = query.value("freq").toDouble();
        row.unit      = query.value("unit").toString();
        row.bw        = query.value("bw").toString();
        row.mode      = query.value("mode").toString();
        row.low_cut   = query.value("low_cut").toInt();
        row.high_cut  = query.value("high_cut").toInt();
        row.path      = query.value("path").toString();
        row.time = query.value("time").toDateTime();
        result.push_back(row);

        // ==== Build JSON Object ====
        QJsonObject obj;
        obj["id"]        = row.id;
        obj["frequency"] = row.freq;
        obj["unit"]      = row.unit;
        obj["bw"]        = row.bw;
        obj["mode"]      = row.mode;
        obj["low_cut"]   = row.low_cut;
        obj["high_cut"]  = row.high_cut;
        obj["path"]      = row.path;
        obj["time"]= row.time.toString(Qt::ISODate);

        jsonArray.append(obj);
    }

    // ==== Emit signals ====
    // emit initValue(result);        // QVector<ScanCard>
    emit initValueJson(jsonArray); // QJsonArray

    db.close();
    qDebug() << "[getAllScanCards] rows:" << result.size();
}
