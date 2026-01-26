#include "databaseiRec.h"
#include <QDateTime>
#include <QStringList>
#include <QString>
#include <QProcess>
#include <QVariant>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonDocument>
#include <typeinfo>
#include <QVariant>

DatabaseiRec::DatabaseiRec(QString dbName, QString user, QString password, QString host, QObject *parent) :
    QObject(parent)
{
    qDebug() << "Connecting to MySQL DatabaseiRec::" << dbName << user << password << host;
//    const QString conn = QString("recorder_%1").arg((quintptr)QThread::currentThreadId());
//    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL", conn);
    const QString connName = QString("irec_%1_%2").arg((quintptr)QThread::currentThreadId()).arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    db = QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName(host);
    db.setDatabaseName(dbName);
    db.setUserName(user);
    db.setPassword(password);
    storageManager = new StorageManagement();
    db.setConnectOptions("MYSQL_OPT_CONNECT_TIMEOUT=5;MYSQL_OPT_READ_TIMEOUT=5;MYSQL_OPT_WRITE_TIMEOUT=5");

    if (!db.open()) {
        qWarning() << "Database connection failed:" << db.lastError().text() << "driverText=" << db.lastError().driverText() << "dbText=" << db.lastError().databaseText() << "connName=" << connName;
        return;
    }

    qDebug() << "Database connected successfully!" << "connName=" << connName << "host=" << db.hostName()<< "db=" << db.databaseName() << "user=" << db.userName();
}

bool DatabaseiRec::execSql(QSqlDatabase& d, const QString& sql, QString* errOut)
{
    QSqlQuery q(d);
    if (!q.exec(sql)) {
        if (errOut) {
            *errOut = q.lastError().text();
        }
        qWarning() << "[VerifyUserDatabase] SQL failed:" << sql << "err=" << q.lastError().text();
        return false;
    }
    return true;
}
bool DatabaseiRec::tryLoginOnce(const QString& user,const QString& host,const QString& dbName,const QString& password,QString* errOut){
    const QString connName = QString("login_%1_%2_%3").arg(user, host, QUuid::createUuid().toString(QUuid::WithoutBraces));

    QSqlDatabase tdb = QSqlDatabase::addDatabase("QMYSQL", connName);
    tdb.setHostName(host);
    tdb.setDatabaseName(dbName);
    tdb.setUserName(user);
    tdb.setPassword(password);
    tdb.setConnectOptions("MYSQL_OPT_CONNECT_TIMEOUT=3;MYSQL_OPT_READ_TIMEOUT=3;MYSQL_OPT_WRITE_TIMEOUT=3");

    if (!tdb.open()) {
        if (errOut) {
            QSqlError e = tdb.lastError();
            *errOut = QString("LOGIN FAIL user=%1 host=%2 db=%3 | %4")
                          .arg(user, host, dbName, e.text());
        }
        tdb.close();
        tdb = QSqlDatabase();
        QSqlDatabase::removeDatabase(connName);
        return false;
    }

    QSqlQuery q(tdb);
    if (!q.exec("SELECT 1")) {
        if (errOut) *errOut = QString("QUERY FAIL user=%1 host=%2 | %3").arg(user, host, q.lastError().text());
        tdb.close();
        tdb = QSqlDatabase();
        QSqlDatabase::removeDatabase(connName);
        return false;
    }

    tdb.close();
    tdb = QSqlDatabase();
    QSqlDatabase::removeDatabase(connName);
    return true;
}
bool DatabaseiRec::runMysqlBootstrapAsRoot(QString* errOut)
{
    const QByteArray sql = R"SQL(
CREATE DATABASE IF NOT EXISTS `recorder`;

CREATE USER IF NOT EXISTS 'recorder'@'localhost' IDENTIFIED BY 'Ifz8zean6868**';
CREATE USER IF NOT EXISTS 'recorder'@'127.0.0.1' IDENTIFIED BY 'Ifz8zean6868**';
CREATE USER IF NOT EXISTS 'recorder'@'%'         IDENTIFIED BY 'Ifz8zean6868**';

CREATE USER IF NOT EXISTS 'iScreenKraken'@'localhost' IDENTIFIED BY 'Ifz8zean6868**';
CREATE USER IF NOT EXISTS 'iScreenKraken'@'127.0.0.1' IDENTIFIED BY 'Ifz8zean6868**';
CREATE USER IF NOT EXISTS 'iScreenKraken'@'%'         IDENTIFIED BY 'Ifz8zean6868**';

ALTER USER 'recorder'@'localhost' IDENTIFIED WITH mysql_native_password BY 'Ifz8zean6868**';
ALTER USER 'recorder'@'127.0.0.1' IDENTIFIED WITH mysql_native_password BY 'Ifz8zean6868**';
ALTER USER 'recorder'@'%'         IDENTIFIED WITH mysql_native_password BY 'Ifz8zean6868**';

ALTER USER 'iScreenKraken'@'localhost' IDENTIFIED WITH mysql_native_password BY 'Ifz8zean6868**';
ALTER USER 'iScreenKraken'@'127.0.0.1' IDENTIFIED WITH mysql_native_password BY 'Ifz8zean6868**';
ALTER USER 'iScreenKraken'@'%'         IDENTIFIED WITH mysql_native_password BY 'Ifz8zean6868**';

GRANT ALL PRIVILEGES ON `recorder`.* TO 'recorder'@'localhost';
GRANT ALL PRIVILEGES ON `recorder`.* TO 'recorder'@'127.0.0.1';
GRANT ALL PRIVILEGES ON `recorder`.* TO 'recorder'@'%';

GRANT ALL PRIVILEGES ON `recorder`.* TO 'iScreenKraken'@'localhost';
GRANT ALL PRIVILEGES ON `recorder`.* TO 'iScreenKraken'@'127.0.0.1';
GRANT ALL PRIVILEGES ON `recorder`.* TO 'iScreenKraken'@'%';

FLUSH PRIVILEGES;
)SQL";

    // 1) Write SQL to temp file
    QTemporaryFile tf("/tmp/irec_bootstrap_mysql_XXXXXX.sql");
    tf.setAutoRemove(true);

    if (!tf.open()) {
        if (errOut) *errOut = "Cannot create temp SQL file in /tmp";
        return false;
    }
    if (tf.write(sql) != sql.size()) {
        if (errOut) *errOut = "Failed to write SQL to temp file";
        return false;
    }
    tf.flush();

    const QString sqlPath = tf.fileName();

    // 2) Run: sudo -n mysql < file.sql
    QProcess p;
    // ใช้ bash -lc เพื่อให้ redirection "< file" ทำงาน
    const QString cmd = QString("sudo -n /usr/bin/mysql < %1").arg(sqlPath);

    p.start("/bin/bash", QStringList() << "-lc" << cmd);

    if (!p.waitForFinished(60000)) {
        if (errOut) *errOut = "mysql bootstrap timeout";
        return false;
    }

    const QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    const QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();

    if (!out.isEmpty())
        qDebug() << "[mysql bootstrap OUT]" << out;
    if (!err.isEmpty())
        qWarning() << "[mysql bootstrap ERR]" << err;

    if (p.exitCode() != 0) {
        if (errOut) {
            *errOut = QString("mysql bootstrap failed (exitCode=%1): %2")
                          .arg(p.exitCode())
                          .arg(err.isEmpty() ? out : err);
        }
        return false;
    }

    return true;
}
bool DatabaseiRec::ensureMysqlUser(QSqlDatabase& d, const QString& user, const QString& host, const QString& password, const QString& dbName, QString* errOut){
    // 1) CREATE USER IF NOT EXISTS
    {
        const QString sql =
            QString("CREATE USER IF NOT EXISTS '%1'@'%2' IDENTIFIED BY '%3';")
                .arg(user, host, password);
        if (!execSql(d, sql, errOut))
            return false;
    }

    // 2) ตั้ง plugin เป็น mysql_native_password + ตั้งรหัสให้ตรง (กันเคส caching_sha2)
    {
        const QString sql =
            QString("ALTER USER '%1'@'%2' IDENTIFIED WITH mysql_native_password BY '%3';")
                .arg(user, host, password);
        if (!execSql(d, sql, errOut))
            return false;
    }

    // 3) GRANT privileges บน dbName.*
    {
        const QString sql =
            QString("GRANT ALL PRIVILEGES ON `%1`.* TO '%2'@'%3';")
                .arg(dbName, user, host);
        if (!execSql(d, sql, errOut))
            return false;
    }

    return true;
}
void DatabaseiRec::VerifyUserDatabase()
{
    const QString dbName = "recorder";
    const QString password = "Ifz8zean6868**";

    // 1) Test iScreenKraken (เพื่อดูว่า DB พื้นฐานใช้ได้)
    QString err;
    bool okIScreen = tryLoginOnce("iScreenKraken", "localhost", dbName, password, &err)
                  || tryLoginOnce("iScreenKraken", "127.0.0.1", dbName, password, &err);

    if (!okIScreen) {
        emit verifyUserDatabaseDone(false,
            "VerifyUserDatabase FAILED: iScreenKraken cannot login. DB/Password/Service MySQL may be broken.\n" + err);
        return;
    }

    // 2) Test recorder (ตัวที่ irecd ต้องใช้)
    bool okRecorder = tryLoginOnce("recorder", "localhost", dbName, password, &err)
                   || tryLoginOnce("recorder", "127.0.0.1", dbName, password, &err);

    if (okRecorder) {
        emit verifyUserDatabaseDone(true, "VerifyUserDatabase OK: recorder can login (no bootstrap needed).");
        return;
    }

    // 3) recorder login ไม่ได้ -> ทำ self-heal (bootstrap) ผ่าน sudo mysql
    qWarning() << "[VerifyUserDatabase] recorder missing/invalid -> bootstrap required:" << err;

    QString bootErr;
    if (!runMysqlBootstrapAsRoot(&bootErr)) {
        emit verifyUserDatabaseDone(false,
            "VerifyUserDatabase FAILED: recorder cannot login and bootstrap failed.\n"
            "Reason: " + err + "\n"
            "Bootstrap error: " + bootErr + "\n"
            "Fix needed: allow sudo -n mysql for this app/service (sudoers).");
        return;
    }

    // 4) Test recorder อีกครั้งหลัง bootstrap
    QString err2;
    okRecorder = tryLoginOnce("recorder", "localhost", dbName, password, &err2)
              || tryLoginOnce("recorder", "127.0.0.1", dbName, password, &err2);

    if (!okRecorder) {
        emit verifyUserDatabaseDone(false,
            "VerifyUserDatabase FAILED: bootstrap ran but recorder still cannot login.\n" + err2);
        return;
    }

    emit verifyUserDatabaseDone(true, "VerifyUserDatabase OK: bootstrap fixed recorder login.");
}

void DatabaseiRec::getCurrentPath() {
    qDebug() << "getCurrentPath:";

    if (!db.isOpen() && !db.open()) {
        const QString err = QStringLiteral("Failed to open database: %1").arg(db.lastError().text());
        qWarning() << err;
        return;
    }

    QSqlQuery q(db);
    // ถ้า schema คุณ fix เป็น id=1:
    if (!q.exec(QStringLiteral("SELECT currentpath FROM current_path WHERE id=1"))) {
        const QString err = QStringLiteral("Query failed: %1").arg(q.lastError().text());
        qWarning() << err;
        return;
    }

    if (q.next()) {
        const QString path = q.value(0).toString().trimmed();
        qDebug() << "currentpath =" << path;
        emit currentPathFetched(path);           // <<<< ส่งค่าออกไป
    } else {
        const QString err = QStringLiteral("No row found in current_path");
        qWarning() << err;
    }
}



bool restartMySQLService() {
    qDebug() << "Restarting MySQL service...";
    QProcess process;
    process.start("systemctl restart mysql");
    process.waitForFinished();
    int exitCode = process.exitCode();

    if (exitCode == 0) {
        qDebug() << "MySQL service restarted successfully.";
        return true;
    } else {
        qWarning() << "Failed to restart MySQL service.";
        return false;
    }
}


void DatabaseiRec::searchRecordFilesMysql(QString msg, QWebSocket* wClient)
{
    qDebug() << "[searchRecordFilesMysql] payload:" << msg;

    // -------- Parse input --------
    const QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
    const QJsonObject in = doc.object();

    const QString tag = in.value("objectName").toString(in.value("menuID").toString());
    const bool isWebRequest = (tag == "searchRecordFilesWeb");

    // ยอมรับได้ 2 แบบ: searchRecordFiles (QML) และ searchRecordFilesWeb (WEB)
    if (tag != "searchRecordFiles" && !isWebRequest) {
        qWarning() << "searchRecordFiles: wrong tag:" << tag;
        return;
    }

    const QString deviceStr  = in.value("device").toString();     // "2"
    const QString startStr   = in.value("startDate").toString();  // "2025-12-03T16:00:06"
    const QString endStr     = in.value("endDate").toString();    // "2025-12-03T17:00:06"
    const QString frequency  = in.value("frequency").toString();  // optional

    const int page     = std::max(1, in.value("page").toInt(1));
    const int pageSize = std::max(1, in.value("pageSize").toInt(25));
    const int offset   = (page - 1) * pageSize;

    // -------- Open DB --------
    if (!db.isValid()) {
        qWarning() << "DB invalid";
        return;
    }
    if (!db.isOpen() && !db.open()) {
        qWarning() << "DB open failed:" << db.lastError().text();
        return;
    }

    // -------- Parse datetime --------
    QDateTime startDT, endDT;

    // start
    startDT = QDateTime::fromString(startStr, Qt::ISODate);
    if (!startDT.isValid()) startDT = QDateTime::fromString(startStr, "MM/dd/yyyy, HH:mm:ss");
    if (!startDT.isValid()) startDT = QDateTime::fromString(startStr, "MM/dd/yyyy HH:mm:ss");
    if (!startDT.isValid()) {
        QLocale us(QLocale::English, QLocale::UnitedStates);
        startDT = us.toDateTime(startStr, "MM/dd/yyyy, hh:mm:ss AP");
        if (!startDT.isValid())
            startDT = us.toDateTime(startStr, "MM/dd/yyyy hh:mm:ss AP");
    }
    if (!startDT.isValid()) startDT = QDateTime::fromString(startStr, "yyyy/MM/dd HH:mm:ss");

    // end
    endDT = QDateTime::fromString(endStr, Qt::ISODate);
    if (!endDT.isValid()) endDT = QDateTime::fromString(endStr, "MM/dd/yyyy, HH:mm:ss");
    if (!endDT.isValid()) endDT = QDateTime::fromString(endStr, "MM/dd/yyyy HH:mm:ss");
    if (!endDT.isValid()) {
        QLocale us(QLocale::English, QLocale::UnitedStates);
        endDT = us.toDateTime(endStr, "MM/dd/yyyy, hh:mm:ss AP");
        if (!endDT.isValid())
            endDT = us.toDateTime(endStr, "MM/dd/yyyy hh:mm:ss AP");
    }
    if (!endDT.isValid()) endDT = QDateTime::fromString(endStr, "yyyy/MM/dd HH:mm:ss");

    if (!startDT.isValid() || !endDT.isValid()) {
        qWarning() << "Invalid start/end date format. start=" << startStr << " end=" << endStr;
        return;
    }

    startDT.setTimeSpec(Qt::LocalTime);
    endDT.setTimeSpec(Qt::LocalTime);

    const QString mysqlStart = startDT.toString("yyyy-MM-dd HH:mm:ss");
    const QString mysqlEnd   = endDT.toString("yyyy-MM-dd HH:mm:ss");

    // -------- COUNT (totalPages) --------
    int totalRows = 0;
    {
        QString countSQL =
            "SELECT COUNT(*) "
            "FROM record_files "
            "WHERE device = :device "
            "  AND created_at BETWEEN :start AND :end ";

        bool fOK = false;
        const double f = frequency.toDouble(&fOK);
        if (!frequency.isEmpty()) {
            countSQL += " AND ( filename LIKE :pat1 OR filename LIKE :pat2 ";
            if (fOK)  countSQL += " OR filename LIKE :pat3 ";
            countSQL += ") ";
        }

        QSqlQuery cq(db);
        if (!cq.prepare(countSQL)) {
            qWarning() << "count prepare:" << cq.lastError().text();
            return;
        }
        cq.bindValue(":device", deviceStr.toInt());
        cq.bindValue(":start",  mysqlStart);
        cq.bindValue(":end",    mysqlEnd);

        if (!frequency.isEmpty()) {
            cq.bindValue(":pat1", "%" + frequency + ".wav");
            cq.bindValue(":pat2", "%_" + frequency + "_%");
            if (fOK) {
                const int kHzTimes1000 = static_cast<int>(f * 1000.0 + 0.5);
                cq.bindValue(":pat3", QString::number(kHzTimes1000) + "_%");
            }
        }

        if (!cq.exec()) {
            qWarning() << "count exec:" << cq.lastError().text();
            return;
        }
        if (cq.next())
            totalRows = cq.value(0).toInt();
    }
    const int totalPages = (totalRows + pageSize - 1) / pageSize;

    // -------- FETCH (page) --------
    QString sql =
        "SELECT id, device, filename, created_at, continuous_count, file_path, name "
        "FROM record_files "
        "WHERE device = :device "
        "  AND created_at BETWEEN :start AND :end ";

    bool fOK = false;
    double f = frequency.toDouble(&fOK);
    if (!frequency.isEmpty()) {
        sql += " AND ( filename LIKE :pat1 OR filename LIKE :pat2 ";
        if (fOK) sql += " OR filename LIKE :pat3 ";
        sql += ") ";
    }

    sql += "ORDER BY created_at DESC "
           "LIMIT :limit OFFSET :offset";

    QSqlQuery q(db);
    if (!q.prepare(sql)) {
        qWarning() << "prepare:" << q.lastError().text();
        return;
    }
    q.bindValue(":device", deviceStr.toInt());
    q.bindValue(":start",  mysqlStart);
    q.bindValue(":end",    mysqlEnd);
    if (!frequency.isEmpty()) {
        q.bindValue(":pat1", "%" + frequency + ".wav");
        q.bindValue(":pat2", "%_" + frequency + "_%");
        if (fOK) {
            const int kHzTimes1000 = static_cast<int>(f * 1000.0 + 0.5);
            q.bindValue(":pat3", QString::number(kHzTimes1000) + "_%");
        }
    }
    q.bindValue(":limit",  pageSize);
    q.bindValue(":offset", offset);

    if (!q.exec()) {
        qWarning() << "exec:" << q.lastError().text();
        return;
    }

    // ===== FAST WAV DURATION =====
    auto fastWavDurationSec = [](const QString &path) -> double {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return -1.0;

        QByteArray hdr = file.read(12);
        if (hdr.size() < 12)
            return -1.0;

        if (memcmp(hdr.constData(), "RIFF", 4) != 0)
            return -1.0;
        if (memcmp(hdr.constData() + 8, "WAVE", 4) != 0)
            return -1.0;

        auto le16 = [](const unsigned char *p) -> quint16 {
            return quint16(p[0]) | (quint16(p[1]) << 8);
        };
        auto le32 = [](const unsigned char *p) -> quint32 {
            return quint32(p[0])
                 | (quint32(p[1]) << 8)
                 | (quint32(p[2]) << 16)
                 | (quint32(p[3]) << 24);
        };

        bool haveFmt  = false;
        bool haveData = false;

        quint16 audioFormat   = 0;
        quint16 numChannels   = 0;
        quint32 sampleRate    = 0;
        quint16 bitsPerSample = 0;
        quint32 dataSize      = 0;

        while (!file.atEnd()) {
            QByteArray chHdr = file.read(8);
            if (chHdr.size() < 8)
                break;

            const unsigned char *ch = reinterpret_cast<const unsigned char*>(chHdr.constData());
            char id[5] = { (char)ch[0], (char)ch[1], (char)ch[2], (char)ch[3], 0 };
            quint32 chunkSize = le32(ch + 4);

            if (chunkSize > 1000000000u)
                break;

            if (strcmp(id, "fmt ") == 0) {
                const quint32 need = qMin(chunkSize, (quint32)32);
                QByteArray fmtData = file.read(need);
                if ((quint32)fmtData.size() < need)
                    break;

                const unsigned char *p = reinterpret_cast<const unsigned char*>(fmtData.constData());
                if (fmtData.size() >= 16) {
                    audioFormat   = le16(p + 0);
                    numChannels   = le16(p + 2);
                    sampleRate    = le32(p + 4);
                    bitsPerSample = le16(p + 14);
                    haveFmt       = true;
                }

                qint64 remain = (qint64)chunkSize - (qint64)fmtData.size();
                if (remain > 0)
                    file.seek(file.pos() + remain);
            }
            else if (strcmp(id, "data") == 0) {
                dataSize  = chunkSize;
                haveData  = true;
                file.seek(file.pos() + chunkSize);
            }
            else {
                file.seek(file.pos() + chunkSize);
            }

            if (haveFmt && haveData)
                break;
        }

        if (!haveFmt || !haveData)
            return -1.0;
        if (sampleRate == 0 || bitsPerSample == 0 || numChannels == 0)
            return -1.0;

        const quint32 bytesPerFrame = (bitsPerSample / 8u) * numChannels;
        if (bytesPerFrame == 0)
            return -1.0;

        const double totalFrames = (double)dataSize / (double)bytesPerFrame;
        const double durationSec = totalFrames / (double)sampleRate;
        if (durationSec < 0.0)
            return -1.0;

        return durationSec;
    };

    // ===== SLOW FALLBACK: ffprobe =====
    auto ffprobeDurationSec = [](const QString &path) -> double {
        QProcess proc;
        QStringList args;
        args << "-v" << "error"
             << "-show_entries" << "format=duration"
             << "-of" << "default=noprint_wrappers=1:nokey=1"
             << path;

        proc.start("ffprobe", args);
        if (!proc.waitForFinished(2000) ||
            proc.exitStatus() != QProcess::NormalExit ||
            proc.exitCode()  != 0) {
            qWarning() << "[ffprobeDurationSec] ffprobe failed for" << path
                       << "err:" << proc.readAllStandardError();
            return -1.0;
        }

        QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        bool ok = false;
        double d = out.toDouble(&ok);
        if (!ok) {
            qWarning() << "[ffprobeDurationSec] parse failed for" << path << "out=" << out;
            return -1.0;
        }
        return d;
    };

    // -------- Pack JSON rows --------
    QJsonArray rows;
    while (q.next()) {
        const QString filename = q.value("filename").toString();
        const QString filePath = q.value("file_path").toString();

        QJsonObject r;
        r["id"]               = q.value("id").toString();
        r["device"]           = q.value("device").toString();
        r["filename"]         = filename;
        r["created_at"]       = q.value("created_at").toString();
        r["continuous_count"] = q.value("continuous_count").toInt();
        r["file_path"]        = filePath;
        r["name"]             = q.value("name").toString();

        const QString fnameNoExt = filename.section('.', 0, 0);
        const QStringList parts  = fnameNoExt.split('_');
        QString deviceName, ymd;
        if (parts.size() >= 2) {
            deviceName = parts[0];
            ymd        = parts[1];
            r["parsed_date"] = ymd;
        }

        const QString fullPath = QDir::cleanPath(filePath + "/" + deviceName + "/" + ymd + "/" + filename);
        r["full_path"] = fullPath;

        QFileInfo fi(fullPath);
        double sizeBytes  = -1.0;
        double sizeKB     = -1.0;
        double durSec     = -1.0;
        QString sizeStr;
        QString durStr;

        if (fi.exists() && fi.isFile()) {
            sizeBytes = static_cast<double>(fi.size());
            sizeKB    = sizeBytes / 1024.0;
            sizeStr   = QString::number(sizeKB, 'f', 3);

            if (filename.endsWith(".wav", Qt::CaseInsensitive)) {
                durSec = fastWavDurationSec(fullPath);
                if (durSec < 0.0)
                    durSec = ffprobeDurationSec(fullPath);

                if (durSec >= 0.0) {
                    durSec = durSec / 2.0; // logic เดิม
                    durStr = QString::number(durSec, 'f', 3);
                }
            }
        }
        if (sizeBytes >= 0.0) {
            r["size_bytes"] = sizeBytes;
            r["size"]       = sizeStr;
        } else {
            r["size_bytes"] = 0.0;
            r["size"]       = "";
        }

        if (durSec >= 0.0) {
            r["duration_sec"] = durSec;
            r["duration_str"] = durStr;
        } else {
            r["duration_sec"] = 0.0;
            r["duration_str"] = "";
        }

        qDebug() << "[rec]" << fullPath
                 << "exists=" << fi.exists()
                 << "sizeBytes=" << sizeBytes
                 << "durSec=" << durSec;

        rows.append(r);
    }

    // -------- ส่งให้ QML (recordFilesChunk + statusSearchFiles) ถ้าไม่ใช่ Web --------
    if (!isWebRequest) {
        QJsonObject out;
        out["objectName"] = "recordFilesChunk";
        out["records"]    = rows;
        out["page"]       = page;
        out["totalPages"] = totalPages;
        out["isLast"]     = (page >= totalPages);

        const QString payload = QJsonDocument(out).toJson(QJsonDocument::Compact);
        emit commandMysqlToCpp(payload);

        qDebug() << "[searchRecordFilesMysql] page:" << page
                 << "rows:" << rows.size()
                 << "totalPages:" << totalPages
                 << "range:" << mysqlStart << "->" << mysqlEnd;

        QJsonObject statusObj;
        statusObj["menuID"] = "statusSearchFiles";
        statusObj["status"] = rows.isEmpty()
                              ? "Files is not found"
                              : "Done";

        const QString statusJson = QJsonDocument(statusObj).toJson(QJsonDocument::Compact);
        qDebug() << "statusJson:" << statusJson;
        emit commandMysqlToCpp(statusJson);
    }

    // -------- ส่งให้ Web (searchRecordFilesResult) ถ้าเป็น Web --------
    if (isWebRequest) {
        QJsonObject replyObj;
        replyObj["menuID"]      = "searchRecordFilesResult";
        replyObj["success"]     = true;
        replyObj["recordCount"] = rows.size();
        replyObj["device"]      = deviceStr.toInt();
        replyObj["records"]     = rows;

        const QString jsonReply = QJsonDocument(replyObj).toJson(QJsonDocument::Compact);
        qDebug() << "[searchRecordFilesMysql] Web reply:" << jsonReply;

        emit commandMysqlToWeb(jsonReply);   // <-- ตรงนี้จะไป ChatServerWebRec::broadcastMessage
    }
}


//void DatabaseiRec::searchRecordFilesMysql(QString msg, QWebSocket* wClient)
//{
//    qDebug() << "[searchRecordFilesMysql] payload:" << msg;

//    // -------- Parse input --------
//    const QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
//    const QJsonObject in = doc.object();

//    const QString tag = in.value("objectName").toString(in.value("menuID").toString());
//    if (tag != "searchRecordFiles") {
//        qWarning() << "searchRecordFiles: wrong tag:" << tag;
//        return;
//    }

//    const QString deviceStr  = in.value("device").toString();     // "2"
//    const QString startStr   = in.value("startDate").toString();  // "09/22/2025, 14:00:39"
//    const QString endStr     = in.value("endDate").toString();    // "09/22/2025, 14:05:39"
//    const QString frequency  = in.value("frequency").toString();  // optional

//    const int page     = std::max(1, in.value("page").toInt(1));
//    const int pageSize = std::max(1, in.value("pageSize").toInt(25));
//    const int offset   = (page - 1) * pageSize;

//    // -------- Open DB --------
//    if (!db.isValid()) {
//        qWarning() << "DB invalid";
//        return;
//    }
//    if (!db.isOpen() && !db.open()) {
//        qWarning() << "DB open failed:" << db.lastError().text();
//        return;
//    }

//    // -------- Parse datetime --------
//    QDateTime startDT, endDT;

//    // start
//    startDT = QDateTime::fromString(startStr, Qt::ISODate);
//    if (!startDT.isValid()) startDT = QDateTime::fromString(startStr, "MM/dd/yyyy, HH:mm:ss");
//    if (!startDT.isValid()) startDT = QDateTime::fromString(startStr, "MM/dd/yyyy HH:mm:ss");
//    if (!startDT.isValid()) {
//        QLocale us(QLocale::English, QLocale::UnitedStates);
//        startDT = us.toDateTime(startStr, "MM/dd/yyyy, hh:mm:ss AP");
//        if (!startDT.isValid())
//            startDT = us.toDateTime(startStr, "MM/dd/yyyy hh:mm:ss AP");
//    }
//    if (!startDT.isValid()) startDT = QDateTime::fromString(startStr, "yyyy/MM/dd HH:mm:ss");

//    // end
//    endDT = QDateTime::fromString(endStr, Qt::ISODate);
//    if (!endDT.isValid()) endDT = QDateTime::fromString(endStr, "MM/dd/yyyy, HH:mm:ss");
//    if (!endDT.isValid()) endDT = QDateTime::fromString(endStr, "MM/dd/yyyy HH:mm:ss");
//    if (!endDT.isValid()) {
//        QLocale us(QLocale::English, QLocale::UnitedStates);
//        endDT = us.toDateTime(endStr, "MM/dd/yyyy, hh:mm:ss AP");
//        if (!endDT.isValid())
//            endDT = us.toDateTime(endStr, "MM/dd/yyyy hh:mm:ss AP");
//    }
//    if (!endDT.isValid()) endDT = QDateTime::fromString(endStr, "yyyy/MM/dd HH:mm:ss");

//    if (!startDT.isValid() || !endDT.isValid()) {
//        qWarning() << "Invalid start/end date format. start=" << startStr << " end=" << endStr;
//        return;
//    }

//    startDT.setTimeSpec(Qt::LocalTime);
//    endDT.setTimeSpec(Qt::LocalTime);

//    const QString mysqlStart = startDT.toString("yyyy-MM-dd HH:mm:ss");
//    const QString mysqlEnd   = endDT.toString("yyyy-MM-dd HH:mm:ss");

//    // -------- COUNT (totalPages) --------
//    int totalRows = 0;
//    {
//        QString countSQL =
//            "SELECT COUNT(*) "
//            "FROM record_files "
//            "WHERE device = :device "
//            "  AND created_at BETWEEN :start AND :end ";

//        bool fOK = false;
//        const double f = frequency.toDouble(&fOK);
//        if (!frequency.isEmpty()) {
//            countSQL += " AND ( filename LIKE :pat1 OR filename LIKE :pat2 ";
//            if (fOK)  countSQL += " OR filename LIKE :pat3 ";
//            countSQL += ") ";
//        }

//        QSqlQuery cq(db);
//        if (!cq.prepare(countSQL)) {
//            qWarning() << "count prepare:" << cq.lastError().text();
//            return;
//        }
//        cq.bindValue(":device", deviceStr.toInt());
//        cq.bindValue(":start",  mysqlStart);
//        cq.bindValue(":end",    mysqlEnd);

//        if (!frequency.isEmpty()) {
//            cq.bindValue(":pat1", "%" + frequency + ".wav");
//            cq.bindValue(":pat2", "%_" + frequency + "_%");
//            if (fOK) {
//                const int kHzTimes1000 = static_cast<int>(f * 1000.0 + 0.5);
//                cq.bindValue(":pat3", QString::number(kHzTimes1000) + "_%");
//            }
//        }

//        if (!cq.exec()) {
//            qWarning() << "count exec:" << cq.lastError().text();
//            return;
//        }
//        if (cq.next())
//            totalRows = cq.value(0).toInt();
//    }
//    const int totalPages = (totalRows + pageSize - 1) / pageSize;

//    // -------- FETCH (page) --------
//    QString sql =
//        "SELECT id, device, filename, created_at, continuous_count, file_path, name "
//        "FROM record_files "
//        "WHERE device = :device "
//        "  AND created_at BETWEEN :start AND :end ";

//    bool fOK = false;
//    double f = frequency.toDouble(&fOK);
//    if (!frequency.isEmpty()) {
//        sql += " AND ( filename LIKE :pat1 OR filename LIKE :pat2 ";
//        if (fOK) sql += " OR filename LIKE :pat3 ";
//        sql += ") ";
//    }

//    sql += "ORDER BY created_at DESC "
//           "LIMIT :limit OFFSET :offset";

//    QSqlQuery q(db);
//    if (!q.prepare(sql)) {
//        qWarning() << "prepare:" << q.lastError().text();
//        return;
//    }
//    q.bindValue(":device", deviceStr.toInt());
//    q.bindValue(":start",  mysqlStart);
//    q.bindValue(":end",    mysqlEnd);
//    if (!frequency.isEmpty()) {
//        q.bindValue(":pat1", "%" + frequency + ".wav");
//        q.bindValue(":pat2", "%_" + frequency + "_%");
//        if (fOK) {
//            const int kHzTimes1000 = static_cast<int>(f * 1000.0 + 0.5);
//            q.bindValue(":pat3", QString::number(kHzTimes1000) + "_%");
//        }
//    }
//    q.bindValue(":limit",  pageSize);
//    q.bindValue(":offset", offset);

//    if (!q.exec()) {
//        qWarning() << "exec:" << q.lastError().text();
//        return;
//    }

//    // ===== FAST WAV DURATION (อ่าน header เอง) =====
//    auto fastWavDurationSec = [](const QString &path) -> double {
//        QFile file(path);
//        if (!file.open(QIODevice::ReadOnly))
//            return -1.0;

//        QByteArray hdr = file.read(12);
//        if (hdr.size() < 12)
//            return -1.0;

//        if (memcmp(hdr.constData(), "RIFF", 4) != 0)
//            return -1.0;
//        if (memcmp(hdr.constData() + 8, "WAVE", 4) != 0)
//            return -1.0;

//        auto le16 = [](const unsigned char *p) -> quint16 {
//            return quint16(p[0]) | (quint16(p[1]) << 8);
//        };
//        auto le32 = [](const unsigned char *p) -> quint32 {
//            return quint32(p[0])
//                 | (quint32(p[1]) << 8)
//                 | (quint32(p[2]) << 16)
//                 | (quint32(p[3]) << 24);
//        };

//        bool haveFmt  = false;
//        bool haveData = false;

//        quint16 audioFormat   = 0;
//        quint16 numChannels   = 0;
//        quint32 sampleRate    = 0;
//        quint16 bitsPerSample = 0;
//        quint32 dataSize      = 0;

//        while (!file.atEnd()) {
//            QByteArray chHdr = file.read(8);
//            if (chHdr.size() < 8)
//                break;

//            const unsigned char *ch = reinterpret_cast<const unsigned char*>(chHdr.constData());
//            char id[5] = { (char)ch[0], (char)ch[1], (char)ch[2], (char)ch[3], 0 };
//            quint32 chunkSize = le32(ch + 4);

//            if (chunkSize > 1000000000u)
//                break;

//            if (strcmp(id, "fmt ") == 0) {
//                const quint32 need = qMin(chunkSize, (quint32)32);
//                QByteArray fmtData = file.read(need);
//                if ((quint32)fmtData.size() < need)
//                    break;

//                const unsigned char *p = reinterpret_cast<const unsigned char*>(fmtData.constData());
//                if (fmtData.size() >= 16) {
//                    audioFormat   = le16(p + 0);
//                    numChannels   = le16(p + 2);
//                    sampleRate    = le32(p + 4);
//                    bitsPerSample = le16(p + 14);
//                    haveFmt       = true;
//                }

//                qint64 remain = (qint64)chunkSize - (qint64)fmtData.size();
//                if (remain > 0)
//                    file.seek(file.pos() + remain);
//            }
//            else if (strcmp(id, "data") == 0) {
//                dataSize  = chunkSize;
//                haveData  = true;
//                file.seek(file.pos() + chunkSize);
//            }
//            else {
//                file.seek(file.pos() + chunkSize);
//            }

//            if (haveFmt && haveData)
//                break;
//        }

//        if (!haveFmt || !haveData)
//            return -1.0;
//        if (sampleRate == 0 || bitsPerSample == 0 || numChannels == 0)
//            return -1.0;

//        const quint32 bytesPerFrame = (bitsPerSample / 8u) * numChannels;
//        if (bytesPerFrame == 0)
//            return -1.0;

//        const double totalFrames = (double)dataSize / (double)bytesPerFrame;
//        const double durationSec = totalFrames / (double)sampleRate;
//        if (durationSec < 0.0)
//            return -1.0;

//        return durationSec;
//    };

//    // ===== SLOW FALLBACK: ffprobe =====
//    auto ffprobeDurationSec = [](const QString &path) -> double {
//        QProcess proc;
//        QStringList args;
//        args << "-v" << "error"
//             << "-show_entries" << "format=duration"
//             << "-of" << "default=noprint_wrappers=1:nokey=1"
//             << path;

//        proc.start("ffprobe", args);
//        if (!proc.waitForFinished(2000) ||
//            proc.exitStatus() != QProcess::NormalExit ||
//            proc.exitCode()  != 0) {
//            qWarning() << "[ffprobeDurationSec] ffprobe failed for" << path
//                       << "err:" << proc.readAllStandardError();
//            return -1.0;
//        }

//        QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
//        bool ok = false;
//        double d = out.toDouble(&ok);
//        if (!ok) {
//            qWarning() << "[ffprobeDurationSec] parse failed for" << path << "out=" << out;
//            return -1.0;
//        }
//        return d;
//    };

//    // -------- Pack JSON rows --------
//    QJsonArray rows;
//    while (q.next()) {
//        const QString filename = q.value("filename").toString();
//        const QString filePath = q.value("file_path").toString();

//        QJsonObject r;
//        r["id"]               = q.value("id").toString();
//        r["device"]           = q.value("device").toString();
//        r["filename"]         = filename;
//        r["created_at"]       = q.value("created_at").toString();
//        r["continuous_count"] = q.value("continuous_count").toInt();
//        r["file_path"]        = filePath;
//        r["name"]             = q.value("name").toString();

//        const QString fnameNoExt = filename.section('.', 0, 0);
//        const QStringList parts  = fnameNoExt.split('_');
//        QString deviceName, ymd;
//        if (parts.size() >= 2) {
//            deviceName = parts[0];
//            ymd        = parts[1];
//            r["parsed_date"] = ymd;
//        }

//        const QString fullPath = QDir::cleanPath(filePath + "/" + deviceName + "/" + ymd + "/" + filename);
//        r["full_path"] = fullPath;

//        QFileInfo fi(fullPath);
//        double sizeBytes  = -1.0;
//        double sizeKB     = -1.0;
//        double durSec     = -1.0;
//        QString sizeStr;
//        QString durStr;

//        if (fi.exists() && fi.isFile()) {
//            sizeBytes = static_cast<double>(fi.size());
//            sizeKB    = sizeBytes / 1024.0;
//            sizeStr   = QString::number(sizeKB, 'f', 3);

//            if (filename.endsWith(".wav", Qt::CaseInsensitive)) {
//                durSec = fastWavDurationSec(fullPath);
//                if (durSec < 0.0)
//                    durSec = ffprobeDurationSec(fullPath);

//                if (durSec >= 0.0) {
//                    durSec = durSec / 2.0; // ตาม logic เดิมของคุณ
//                    durStr = QString::number(durSec, 'f', 3);
//                }
//            }
//        }
//        if (sizeBytes >= 0.0) {
//            r["size_bytes"] = sizeBytes;
//            r["size"]       = sizeStr;
//        } else {
//            r["size_bytes"] = 0.0;
//            r["size"]       = "";
//        }

//        if (durSec >= 0.0) {
//            r["duration_sec"] = durSec;
//            r["duration_str"] = durStr;
//        } else {
//            r["duration_sec"] = 0.0;
//            r["duration_str"] = "";
//        }

//        qDebug() << "[rec]" << fullPath
//                 << "exists=" << fi.exists()
//                 << "sizeBytes=" << sizeBytes
//                 << "durSec=" << durSec;

//        rows.append(r);
//    }

//    // -------- Reply: recordFilesChunk (เดิมสำหรับ QML) --------
//    {
//        QJsonObject out;
//        out["objectName"] = "recordFilesChunk";
//        out["records"]    = rows;
//        out["page"]       = page;
//        out["totalPages"] = totalPages;
//        out["isLast"]     = (page >= totalPages);

//        const QString payload = QJsonDocument(out).toJson(QJsonDocument::Compact);
//        emit commandMysqlToCpp(payload);

//        qDebug() << "[searchRecordFilesMysql] page:" << page
//                 << "rows:" << rows.size()
//                 << "totalPages:" << totalPages
//                 << "range:" << mysqlStart << "->" << mysqlEnd;
//    }

//    // -------- Reply: statusSearchFiles (เดิมสำหรับ QML) --------
//    {
//        QJsonObject statusObj;
//        statusObj["menuID"] = "statusSearchFiles";

//        if (rows.isEmpty()) {
//            // ถ้าไม่พบไฟล์ในช่วงเวลานั้นเลย
//            statusObj["status"] = "Files is not found";
//        } else {
//            statusObj["status"] = "Done";
//        }

//        const QString statusJson = QJsonDocument(statusObj).toJson(QJsonDocument::Compact);
//        qDebug() << "statusJson:" << statusJson;
//        emit commandMysqlToCpp(statusJson);
//    }

//    // ========= NEW: ส่งแบบ searchRecordFilesResult ไปยัง WebSocket (wClient) =========
//    if (wClient) {
//        QJsonObject replyObj;
//        replyObj["menuID"]      = "searchRecordFilesResult";
//        replyObj["success"]     = true;
//        replyObj["recordCount"] = rows.size();
//        replyObj["device"]      = deviceStr.toInt();
//        replyObj["records"]     = rows;   // ใช้ rows เดิมเลย (web ใช้ field เท่าที่ต้องการ)

//        const QString jsonReply = QJsonDocument(replyObj).toJson(QJsonDocument::Compact);
//        qDebug() << "[searchRecordFilesMysql] Web reply:" << jsonReply << wClient;
////        wClient->sendTextMessage(jsonReply);
////        emit commandMysqlToWeb(wClient,jsonReply);
//    } else {
//        qWarning() << "[searchRecordFilesMysql] wClient is null, skip web reply.";
//    }
//}


//void DatabaseiRec::searchRecordFilesMysql(QString msg, QWebSocket* wClient)
//{
//    qDebug() << "[searchRecordFilesMysql] payload:" << msg;

//    // -------- Parse input --------
//    const QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
//    const QJsonObject in = doc.object();

//    const QString tag = in.value("objectName").toString(in.value("menuID").toString());
//    if (tag != "searchRecordFiles") {
//        qWarning() << "searchRecordFiles: wrong tag:" << tag;
//        return;
//    }

//    const QString deviceStr  = in.value("device").toString();     // "2"
//    const QString startStr   = in.value("startDate").toString();  // "09/22/2025, 14:00:39"
//    const QString endStr     = in.value("endDate").toString();    // "09/22/2025, 14:05:39"
//    const QString frequency  = in.value("frequency").toString();  // optional

//    const int page     = std::max(1, in.value("page").toInt(1));
//    const int pageSize = std::max(1, in.value("pageSize").toInt(25));
//    const int offset   = (page - 1) * pageSize;

//    // -------- Open DB --------
//    if (!db.isValid()) {
//        qWarning() << "DB invalid";
//        return;
//    }
//    if (!db.isOpen() && !db.open()) {
//        qWarning() << "DB open failed:" << db.lastError().text();
//        return;
//    }

//    // -------- Parse datetime --------
//    QDateTime startDT, endDT;

//    // start
//    startDT = QDateTime::fromString(startStr, Qt::ISODate);
//    if (!startDT.isValid()) startDT = QDateTime::fromString(startStr, "MM/dd/yyyy, HH:mm:ss");
//    if (!startDT.isValid()) startDT = QDateTime::fromString(startStr, "MM/dd/yyyy HH:mm:ss");
//    if (!startDT.isValid()) {
//        QLocale us(QLocale::English, QLocale::UnitedStates);
//        startDT = us.toDateTime(startStr, "MM/dd/yyyy, hh:mm:ss AP");
//        if (!startDT.isValid())
//            startDT = us.toDateTime(startStr, "MM/dd/yyyy hh:mm:ss AP");
//    }
//    if (!startDT.isValid()) startDT = QDateTime::fromString(startStr, "yyyy/MM/dd HH:mm:ss");

//    // end
//    endDT = QDateTime::fromString(endStr, Qt::ISODate);
//    if (!endDT.isValid()) endDT = QDateTime::fromString(endStr, "MM/dd/yyyy, HH:mm:ss");
//    if (!endDT.isValid()) endDT = QDateTime::fromString(endStr, "MM/dd/yyyy HH:mm:ss");
//    if (!endDT.isValid()) {
//        QLocale us(QLocale::English, QLocale::UnitedStates);
//        endDT = us.toDateTime(endStr, "MM/dd/yyyy, hh:mm:ss AP");
//        if (!endDT.isValid())
//            endDT = us.toDateTime(endStr, "MM/dd/yyyy hh:mm:ss AP");
//    }
//    if (!endDT.isValid()) endDT = QDateTime::fromString(endStr, "yyyy/MM/dd HH:mm:ss");

//    if (!startDT.isValid() || !endDT.isValid()) {
//        qWarning() << "Invalid start/end date format. start=" << startStr << " end=" << endStr;
//        return;
//    }

//    startDT.setTimeSpec(Qt::LocalTime);
//    endDT.setTimeSpec(Qt::LocalTime);

//    const QString mysqlStart = startDT.toString("yyyy-MM-dd HH:mm:ss");
//    const QString mysqlEnd   = endDT.toString("yyyy-MM-dd HH:mm:ss");

//    // -------- COUNT (totalPages) --------
//    int totalRows = 0;
//    {
//        QString countSQL =
//            "SELECT COUNT(*) "
//            "FROM record_files "
//            "WHERE device = :device "
//            "  AND created_at BETWEEN :start AND :end ";

//        bool fOK = false;
//        const double f = frequency.toDouble(&fOK);
//        if (!frequency.isEmpty()) {
//            countSQL += " AND ( filename LIKE :pat1 OR filename LIKE :pat2 ";
//            if (fOK)  countSQL += " OR filename LIKE :pat3 ";
//            countSQL += ") ";
//        }

//        QSqlQuery cq(db);
//        if (!cq.prepare(countSQL)) {
//            qWarning() << "count prepare:" << cq.lastError().text();
//            return;
//        }
//        cq.bindValue(":device", deviceStr.toInt());
//        cq.bindValue(":start",  mysqlStart);
//        cq.bindValue(":end",    mysqlEnd);

//        if (!frequency.isEmpty()) {
//            cq.bindValue(":pat1", "%" + frequency + ".wav");
//            cq.bindValue(":pat2", "%_" + frequency + "_%");
//            if (fOK) {
//                const int kHzTimes1000 = static_cast<int>(f * 1000.0 + 0.5);
//                cq.bindValue(":pat3", QString::number(kHzTimes1000) + "_%");
//            }
//        }

//        if (!cq.exec()) {
//            qWarning() << "count exec:" << cq.lastError().text();
//            return;
//        }
//        if (cq.next())
//            totalRows = cq.value(0).toInt();
//    }
//    const int totalPages = (totalRows + pageSize - 1) / pageSize;

//    // -------- FETCH (page) --------
//    QString sql =
//        "SELECT id, device, filename, created_at, continuous_count, file_path, name "
//        "FROM record_files "
//        "WHERE device = :device "
//        "  AND created_at BETWEEN :start AND :end ";

//    bool fOK = false;
//    double f = frequency.toDouble(&fOK);
//    if (!frequency.isEmpty()) {
//        sql += " AND ( filename LIKE :pat1 OR filename LIKE :pat2 ";
//        if (fOK) sql += " OR filename LIKE :pat3 ";
//        sql += ") ";
//    }

//    sql += "ORDER BY created_at DESC "
//           "LIMIT :limit OFFSET :offset";

//    QSqlQuery q(db);
//    if (!q.prepare(sql)) {
//        qWarning() << "prepare:" << q.lastError().text();
//        return;
//    }
//    q.bindValue(":device", deviceStr.toInt());
//    q.bindValue(":start",  mysqlStart);
//    q.bindValue(":end",    mysqlEnd);
//    if (!frequency.isEmpty()) {
//        q.bindValue(":pat1", "%" + frequency + ".wav");
//        q.bindValue(":pat2", "%_" + frequency + "_%");
//        if (fOK) {
//            const int kHzTimes1000 = static_cast<int>(f * 1000.0 + 0.5);
//            q.bindValue(":pat3", QString::number(kHzTimes1000) + "_%");
//        }
//    }
//    q.bindValue(":limit",  pageSize);
//    q.bindValue(":offset", offset);

//    if (!q.exec()) {
//        qWarning() << "exec:" << q.lastError().text();
//        return;
//    }

//    // ===== FAST WAV DURATION (อ่าน header เอง) =====
//    auto fastWavDurationSec = [](const QString &path) -> double {
//        QFile file(path);
//        if (!file.open(QIODevice::ReadOnly))
//            return -1.0;

//        QByteArray hdr = file.read(12);
//        if (hdr.size() < 12)
//            return -1.0;

//        if (memcmp(hdr.constData(), "RIFF", 4) != 0)
//            return -1.0;
//        if (memcmp(hdr.constData() + 8, "WAVE", 4) != 0)
//            return -1.0;

//        auto le16 = [](const unsigned char *p) -> quint16 {
//            return quint16(p[0]) | (quint16(p[1]) << 8);
//        };
//        auto le32 = [](const unsigned char *p) -> quint32 {
//            return quint32(p[0])
//                 | (quint32(p[1]) << 8)
//                 | (quint32(p[2]) << 16)
//                 | (quint32(p[3]) << 24);
//        };

//        bool haveFmt  = false;
//        bool haveData = false;

//        quint16 audioFormat   = 0;
//        quint16 numChannels   = 0;
//        quint32 sampleRate    = 0;
//        quint16 bitsPerSample = 0;
//        quint32 dataSize      = 0;

//        while (!file.atEnd()) {
//            QByteArray chHdr = file.read(8);
//            if (chHdr.size() < 8)
//                break;

//            const unsigned char *ch = reinterpret_cast<const unsigned char*>(chHdr.constData());
//            char id[5] = { (char)ch[0], (char)ch[1], (char)ch[2], (char)ch[3], 0 };
//            quint32 chunkSize = le32(ch + 4);

//            if (chunkSize > 1000000000u)
//                break;

//            if (strcmp(id, "fmt ") == 0) {
//                const quint32 need = qMin(chunkSize, (quint32)32);
//                QByteArray fmtData = file.read(need);
//                if ((quint32)fmtData.size() < need)
//                    break;

//                const unsigned char *p = reinterpret_cast<const unsigned char*>(fmtData.constData());
//                if (fmtData.size() >= 16) {
//                    audioFormat   = le16(p + 0);
//                    numChannels   = le16(p + 2);
//                    sampleRate    = le32(p + 4);
//                    bitsPerSample = le16(p + 14);
//                    haveFmt       = true;
//                }

//                qint64 remain = (qint64)chunkSize - (qint64)fmtData.size();
//                if (remain > 0)
//                    file.seek(file.pos() + remain);
//            }
//            else if (strcmp(id, "data") == 0) {
//                dataSize  = chunkSize;
//                haveData  = true;
//                file.seek(file.pos() + chunkSize);
//            }
//            else {
//                file.seek(file.pos() + chunkSize);
//            }

//            if (haveFmt && haveData)
//                break;
//        }

//        if (!haveFmt || !haveData)
//            return -1.0;
//        if (sampleRate == 0 || bitsPerSample == 0 || numChannels == 0)
//            return -1.0;

//        const quint32 bytesPerFrame = (bitsPerSample / 8u) * numChannels;
//        if (bytesPerFrame == 0)
//            return -1.0;

//        const double totalFrames = (double)dataSize / (double)bytesPerFrame;
//        const double durationSec = totalFrames / (double)sampleRate;
//        if (durationSec < 0.0)
//            return -1.0;

//        return durationSec;
//    };

//    // ===== SLOW FALLBACK: ffprobe =====
//    auto ffprobeDurationSec = [](const QString &path) -> double {
//        QProcess proc;
//        QStringList args;
//        args << "-v" << "error"
//             << "-show_entries" << "format=duration"
//             << "-of" << "default=noprint_wrappers=1:nokey=1"
//             << path;

//        proc.start("ffprobe", args);
//        if (!proc.waitForFinished(2000) ||
//            proc.exitStatus() != QProcess::NormalExit ||
//            proc.exitCode()  != 0) {
//            qWarning() << "[ffprobeDurationSec] ffprobe failed for" << path
//                       << "err:" << proc.readAllStandardError();
//            return -1.0;
//        }

//        QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
//        bool ok = false;
//        double d = out.toDouble(&ok);
//        if (!ok) {
//            qWarning() << "[ffprobeDurationSec] parse failed for" << path << "out=" << out;
//            return -1.0;
//        }
//        return d;
//    };

//    // -------- Pack JSON rows --------
//    QJsonArray rows;
//    while (q.next()) {
//        const QString filename = q.value("filename").toString();
//        const QString filePath = q.value("file_path").toString();

//        QJsonObject r;
//        r["id"]               = q.value("id").toString();
//        r["device"]           = q.value("device").toString();
//        r["filename"]         = filename;
//        r["created_at"]       = q.value("created_at").toString();
//        r["continuous_count"] = q.value("continuous_count").toInt();
//        r["file_path"]        = filePath;
//        r["name"]             = q.value("name").toString();

//        const QString fnameNoExt = filename.section('.', 0, 0);
//        const QStringList parts  = fnameNoExt.split('_');
//        QString deviceName, ymd;
//        if (parts.size() >= 2) {
//            deviceName = parts[0];
//            ymd        = parts[1];
//            r["parsed_date"] = ymd;
//        }

//        const QString fullPath = QDir::cleanPath(filePath + "/" + deviceName + "/" + ymd + "/" + filename);
//        r["full_path"] = fullPath;

//        QFileInfo fi(fullPath);
//        double sizeBytes  = -1.0;
//        double sizeKB     = -1.0;
//        double durSec     = -1.0;
//        QString sizeStr;
//        QString durStr;

//        if (fi.exists() && fi.isFile()) {
//            sizeBytes = static_cast<double>(fi.size());
//            sizeKB    = sizeBytes / 1024.0;
//            sizeStr   = QString::number(sizeKB, 'f', 3);

//            if (filename.endsWith(".wav", Qt::CaseInsensitive)) {
//                durSec = fastWavDurationSec(fullPath);
//                if (durSec < 0.0)
//                    durSec = ffprobeDurationSec(fullPath);

//                if (durSec >= 0.0) {
//                    durSec = durSec / 2.0; // ตาม logic เดิมของคุณ
//                    durStr = QString::number(durSec, 'f', 3);
//                }
//            }
//        }
//        if (sizeBytes >= 0.0) {
//            r["size_bytes"] = sizeBytes;
//            r["size"]       = sizeStr;
//        } else {
//            r["size_bytes"] = 0.0;
//            r["size"]       = "";
//        }

//        if (durSec >= 0.0) {
//            r["duration_sec"] = durSec;
//            r["duration_str"] = durStr;
//        } else {
//            r["duration_sec"] = 0.0;
//            r["duration_str"] = "";
//        }

//        qDebug() << "[rec]" << fullPath
//                 << "exists=" << fi.exists()
//                 << "sizeBytes=" << sizeBytes
//                 << "durSec=" << durSec;

//        rows.append(r);
//    }

//    // -------- Reply: recordFilesChunk --------
//    QJsonObject out;
//    out["objectName"] = "recordFilesChunk";
//    out["records"]    = rows;
//    out["page"]       = page;
//    out["totalPages"] = totalPages;
//    out["isLast"]     = (page >= totalPages);

//    const QString payload = QJsonDocument(out).toJson(QJsonDocument::Compact);
//    emit commandMysqlToCpp(payload);

//    qDebug() << "[searchRecordFilesMysql] page:" << page
//             << "rows:" << rows.size()
//             << "totalPages:" << totalPages
//             << "range:" << mysqlStart << "->" << mysqlEnd;

//    // -------- Reply: statusSearchFiles --------
//    {
//        QJsonObject statusObj;
//        statusObj["menuID"] = "statusSearchFiles";

//        if (rows.isEmpty()) {
//            // ถ้าไม่พบไฟล์ในช่วงเวลานั้นเลย
//            statusObj["status"] = "Files is not found";
//        } else {
//            statusObj["status"] = "Done";
//        }

//        const QString statusJson = QJsonDocument(statusObj).toJson(QJsonDocument::Compact);
//        qDebug() << "statusJson:" << statusJson;
//        emit commandMysqlToCpp(statusJson);
//    }
//}


//void DatabaseiRec::searchRecordFilesMysql(QString msg, QWebSocket* wClient)
//{
//    qDebug() << "[searchRecordFilesMysql] payload:" << msg;
//    // -------- Parse input --------
//    const QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
//    const QJsonObject in = doc.object();

//    const QString tag = in.value("objectName").toString(in.value("menuID").toString());
//    if (tag != "searchRecordFiles") {
//        qWarning() << "searchRecordFiles: wrong tag:" << tag;
//        return;
//    }

//    const QString deviceStr  = in.value("device").toString();     // "2"
//    const QString startStr   = in.value("startDate").toString();  // เช่น "09/22/2025, 14:00:39"
//    const QString endStr     = in.value("endDate").toString();    // เช่น "09/22/2025, 14:05:39"
//    const QString frequency  = in.value("frequency").toString();  // optional

//    const int page     = std::max(1, in.value("page").toInt(1));
//    const int pageSize = std::max(1, in.value("pageSize").toInt(25));
//    const int offset   = (page - 1) * pageSize;

//    // -------- Open DB --------
//    if (!db.isValid()) {
//        qWarning() << "DB invalid";
//        return;
//    }
//    if (!db.isOpen() && !db.open()) {
//        qWarning() << "DB open failed:" << db.lastError().text();
//        return;
//    }

//    // -------- Parse datetime แบบยืดหยุ่น --------
//    QDateTime startDT, endDT;

//    // start
//    startDT = QDateTime::fromString(startStr, Qt::ISODate);
//    if (!startDT.isValid()) startDT = QDateTime::fromString(startStr, "MM/dd/yyyy, HH:mm:ss");
//    if (!startDT.isValid()) startDT = QDateTime::fromString(startStr, "MM/dd/yyyy HH:mm:ss");
//    if (!startDT.isValid()) {
//        QLocale us(QLocale::English, QLocale::UnitedStates);
//        startDT = us.toDateTime(startStr, "MM/dd/yyyy, hh:mm:ss AP");
//        if (!startDT.isValid())
//            startDT = us.toDateTime(startStr, "MM/dd/yyyy hh:mm:ss AP");
//    }
//    if (!startDT.isValid()) startDT = QDateTime::fromString(startStr, "yyyy/MM/dd HH:mm:ss");

//    // end
//    endDT = QDateTime::fromString(endStr, Qt::ISODate);
//    if (!endDT.isValid()) endDT = QDateTime::fromString(endStr, "MM/dd/yyyy, HH:mm:ss");
//    if (!endDT.isValid()) endDT = QDateTime::fromString(endStr, "MM/dd/yyyy HH:mm:ss");
//    if (!endDT.isValid()) {
//        QLocale us(QLocale::English, QLocale::UnitedStates);
//        endDT = us.toDateTime(endStr, "MM/dd/yyyy, hh:mm:ss AP");
//        if (!endDT.isValid())
//            endDT = us.toDateTime(endStr, "MM/dd/yyyy hh:mm:ss AP");
//    }
//    if (!endDT.isValid()) endDT = QDateTime::fromString(endStr, "yyyy/MM/dd HH:mm:ss");

//    if (!startDT.isValid() || !endDT.isValid()) {
//        qWarning() << "Invalid start/end date format. start=" << startStr << " end=" << endStr;
//        return;
//    }

//    startDT.setTimeSpec(Qt::LocalTime);
//    endDT.setTimeSpec(Qt::LocalTime);

//    const QString mysqlStart = startDT.toString("yyyy-MM-dd HH:mm:ss");
//    const QString mysqlEnd   = endDT.toString("yyyy-MM-dd HH:mm:ss");

//    // -------- COUNT (สำหรับ totalPages) --------
//    int totalRows = 0;
//    {
//        QString countSQL =
//            "SELECT COUNT(*) "
//            "FROM record_files "
//            "WHERE device = :device "
//            "  AND created_at BETWEEN :start AND :end ";

//        bool fOK = false;
//        const double f = frequency.toDouble(&fOK);
//        if (!frequency.isEmpty()) {
//            countSQL += " AND ( filename LIKE :pat1 OR filename LIKE :pat2 ";
//            if (fOK)  countSQL += " OR filename LIKE :pat3 ";
//            countSQL += ") ";
//        }

//        QSqlQuery cq(db);
//        if (!cq.prepare(countSQL)) {
//            qWarning() << "count prepare:" << cq.lastError().text();
//            return;
//        }
//        cq.bindValue(":device", deviceStr.toInt());
//        cq.bindValue(":start",  mysqlStart);
//        cq.bindValue(":end",    mysqlEnd);

//        if (!frequency.isEmpty()) {
//            cq.bindValue(":pat1", "%" + frequency + ".wav");   // …_121.950.wav
//            cq.bindValue(":pat2", "%_" + frequency + "_%");
//            if (fOK) {
//                const int kHzTimes1000 = static_cast<int>(f * 1000.0 + 0.5);
//                cq.bindValue(":pat3", QString::number(kHzTimes1000) + "_%");
//            }
//        }

//        if (!cq.exec()) {
//            qWarning() << "count exec:" << cq.lastError().text();
//            return;
//        }
//        if (cq.next())
//            totalRows = cq.value(0).toInt();
//    }
//    const int totalPages = (totalRows + pageSize - 1) / pageSize;

//    // -------- FETCH (หน้า page) --------
//    QString sql =
//        "SELECT id, device, filename, created_at, continuous_count, file_path, name "
//        "FROM record_files "
//        "WHERE device = :device "
//        "  AND created_at BETWEEN :start AND :end ";

//    bool fOK = false;
//    double f = frequency.toDouble(&fOK);
//    if (!frequency.isEmpty()) {
//        sql += " AND ( filename LIKE :pat1 OR filename LIKE :pat2 ";
//        if (fOK) sql += " OR filename LIKE :pat3 ";
//        sql += ") ";
//    }

//    sql += "ORDER BY created_at DESC "
//           "LIMIT :limit OFFSET :offset";

//    QSqlQuery q(db);
//    if (!q.prepare(sql)) {
//        qWarning() << "prepare:" << q.lastError().text();
//        return;
//    }
//    q.bindValue(":device", deviceStr.toInt());
//    q.bindValue(":start",  mysqlStart);
//    q.bindValue(":end",    mysqlEnd);
//    if (!frequency.isEmpty()) {
//        q.bindValue(":pat1", "%" + frequency + ".wav");
//        q.bindValue(":pat2", "%_" + frequency + "_%");
//        if (fOK) {
//            const int kHzTimes1000 = static_cast<int>(f * 1000.0 + 0.5);
//            q.bindValue(":pat3", QString::number(kHzTimes1000) + "_%");
//        }
//    }
//    q.bindValue(":limit",  pageSize);
//    q.bindValue(":offset", offset);

//    if (!q.exec()) {
//        qWarning() << "exec:" << q.lastError().text();
//        return;
//    }

//    // ===== FAST WAV DURATION (อ่าน header เอง) =====
//    auto fastWavDurationSec = [](const QString &path) -> double {
//        QFile file(path);
//        if (!file.open(QIODevice::ReadOnly))
//            return -1.0;

//        // RIFF header 12 bytes
//        QByteArray hdr = file.read(12);
//        if (hdr.size() < 12)
//            return -1.0;

//        if (memcmp(hdr.constData(), "RIFF", 4) != 0)
//            return -1.0;
//        if (memcmp(hdr.constData() + 8, "WAVE", 4) != 0)
//            return -1.0;

//        auto le16 = [](const unsigned char *p) -> quint16 {
//            return quint16(p[0]) | (quint16(p[1]) << 8);
//        };
//        auto le32 = [](const unsigned char *p) -> quint32 {
//            return quint32(p[0])
//                 | (quint32(p[1]) << 8)
//                 | (quint32(p[2]) << 16)
//                 | (quint32(p[3]) << 24);
//        };

//        bool haveFmt  = false;
//        bool haveData = false;

//        quint16 audioFormat   = 0;
//        quint16 numChannels   = 0;
//        quint32 sampleRate    = 0;
//        quint16 bitsPerSample = 0;
//        quint32 dataSize      = 0;

//        while (!file.atEnd()) {
//            QByteArray chHdr = file.read(8);
//            if (chHdr.size() < 8)
//                break;

//            const unsigned char *ch = reinterpret_cast<const unsigned char*>(chHdr.constData());
//            char id[5] = { (char)ch[0], (char)ch[1], (char)ch[2], (char)ch[3], 0 };
//            quint32 chunkSize = le32(ch + 4);

//            // กัน chunkSize เพี้ยนมาก ๆ
//            if (chunkSize > 1000000000u)
//                break;

//            if (strcmp(id, "fmt ") == 0) {
//                // fmt ต้องมีอย่างน้อย 16 bytes
//                const quint32 need = qMin(chunkSize, (quint32)32);
//                QByteArray fmtData = file.read(need);
//                if ((quint32)fmtData.size() < need)
//                    break;

//                const unsigned char *p = reinterpret_cast<const unsigned char*>(fmtData.constData());
//                if (fmtData.size() >= 16) {
//                    audioFormat   = le16(p + 0);
//                    numChannels   = le16(p + 2);
//                    sampleRate    = le32(p + 4);
//                    bitsPerSample = le16(p + 14);
//                    haveFmt       = true;
//                }

//                qint64 remain = (qint64)chunkSize - (qint64)fmtData.size();
//                if (remain > 0)
//                    file.seek(file.pos() + remain);
//            }
//            else if (strcmp(id, "data") == 0) {
//                dataSize  = chunkSize;
//                haveData  = true;
//                file.seek(file.pos() + chunkSize);
//            }
//            else {
//                file.seek(file.pos() + chunkSize);
//            }

//            if (haveFmt && haveData)
//                break;
//        }

//        if (!haveFmt || !haveData)
//            return -1.0;
//        if (sampleRate == 0 || bitsPerSample == 0 || numChannels == 0)
//            return -1.0;

//        const quint32 bytesPerFrame = (bitsPerSample / 8u) * numChannels;
//        if (bytesPerFrame == 0)
//            return -1.0;

//        const double totalFrames = (double)dataSize / (double)bytesPerFrame;
//        const double durationSec = totalFrames / (double)sampleRate;
//        if (durationSec < 0.0)
//            return -1.0;

//        return durationSec;
//    };

//    // ===== SLOW FALLBACK: ffprobe =====
//    auto ffprobeDurationSec = [](const QString &path) -> double {
//        QProcess proc;
//        QStringList args;
//        args << "-v" << "error"
//             << "-show_entries" << "format=duration"
//             << "-of" << "default=noprint_wrappers=1:nokey=1"
//             << path;

//        proc.start("ffprobe", args);
//        if (!proc.waitForFinished(2000) ||
//            proc.exitStatus() != QProcess::NormalExit ||
//            proc.exitCode()  != 0) {
//            qWarning() << "[ffprobeDurationSec] ffprobe failed for" << path
//                       << "err:" << proc.readAllStandardError();
//            return -1.0;
//        }

//        QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
//        bool ok = false;
//        double d = out.toDouble(&ok);
//        if (!ok) {
//            qWarning() << "[ffprobeDurationSec] parse failed for" << path << "out=" << out;
//            return -1.0;
//        }
//        return d;
//    };

//    // -------- Pack JSON --------
//    QJsonArray rows;
//    while (q.next()) {
//        const QString filename = q.value("filename").toString();
//        const QString filePath = q.value("file_path").toString(); // "/var/ivoicex"

//        QJsonObject r;
//        r["id"]               = q.value("id").toString();
//        r["device"]           = q.value("device").toString();
//        r["filename"]         = filename;
//        r["created_at"]       = q.value("created_at").toString();
//        r["continuous_count"] = q.value("continuous_count").toInt();
//        r["file_path"]        = filePath;
//        r["name"]             = q.value("name").toString();

//        const QString fnameNoExt = filename.section('.', 0, 0);
//        const QStringList parts  = fnameNoExt.split('_');
//        QString deviceName, ymd;
//        if (parts.size() >= 2) {
//            deviceName = parts[0];   // "23-iGate23-4"
//            ymd        = parts[1];   // "20250922"
//            r["parsed_date"] = ymd;
//        }

//        QString fullPath = QDir::cleanPath(filePath + "/" + deviceName + "/" + ymd + "/" + filename);
//        r["full_path"] = fullPath;

//        QFileInfo fi(fullPath);
//        double sizeBytes  = -1.0;
//        double sizeKB     = -1.0;
//        double durSec     = -1.0;
//        QString sizeStr;
//        QString durStr;

//        if (fi.exists() && fi.isFile()) {
//            sizeBytes = static_cast<double>(fi.size());
//            sizeKB    = sizeBytes / 1024.0;
//            sizeStr   = QString::number(sizeKB, 'f', 3);  // เช่น "66.293"

//            if (filename.endsWith(".wav", Qt::CaseInsensitive)) {
//                durSec = fastWavDurationSec(fullPath);

//                if (durSec < 0.0)
//                    durSec = ffprobeDurationSec(fullPath);

//                if (durSec >= 0.0) {
//                    durSec = durSec / 2.0;

//                    durStr = QString::number(durSec, 'f', 3);
//                }
//            }
//        }
//        if (sizeBytes >= 0.0) {
//            r["size_bytes"] = sizeBytes;
//            r["size"]       = sizeStr;
//        } else {
//            r["size_bytes"] = 0.0;
//            r["size"]       = "";
//        }

//        if (durSec >= 0.0) {
//            r["duration_sec"] = durSec;
//            r["duration_str"] = durStr;
//        } else {
//            r["duration_sec"] = 0.0;
//            r["duration_str"] = "";
//        }

//        qDebug() << "[rec]" << fullPath
//                 << "exists=" << fi.exists()
//                 << "sizeBytes=" << sizeBytes
//                 << "durSec=" << durSec;

//        rows.append(r);
//    }

//    // -------- Reply --------
//    QJsonObject out;
//    out["objectName"] = "recordFilesChunk";
//    out["records"]    = rows;
//    out["page"]       = page;
//    out["totalPages"] = totalPages;
//    out["isLast"]     = (page >= totalPages);

//    const QString payload = QJsonDocument(out).toJson(QJsonDocument::Compact);

//    emit commandMysqlToCpp(payload);
//    // if (wClient && wClient->isValid()) { wClient->sendTextMessage(payload); }

//    qDebug() << "[searchRecordFilesMysql] page:" << page
//             << "rows:" << rows.size()
//             << "totalPages:" << totalPages
//             << "range:" << mysqlStart << "->" << mysqlEnd;

//    {
//        QJsonObject statusDone;
//        statusDone["menuID"] = "statusSearchFiles";
//        statusDone["status"] = "Done";
//        QString statusJsonDone = QJsonDocument(statusDone).toJson(QJsonDocument::Compact);
//        qDebug() << "statusJsonDone:" << statusJsonDone;
//        emit commandMysqlToCpp(statusJsonDone);
//    }
//}


void DatabaseiRec::upDateTableFileRecord()
{
    qDebug() << "[Database] upDateTableFileRecord() baseline:"
             << "created_at =" << m_lastRecordCreatedAt
             << "id =" << m_lastRecordId;

    // ถ้ายังไม่เคยเรียก fetchAllRecordFiles เลย ก็ยังไม่รู้ baseline
    if (m_lastRecordCreatedAt.isEmpty()) {
        qWarning() << "[upDateTableFileRecord] m_lastRecordCreatedAt is empty,"
                   << "call fetchAllRecordFiles() at least once first.";
        return;
    }

    // ----- เปิด DB -----
    if (!db.isValid()) {
        qWarning() << "[upDateTableFileRecord] Database connection is invalid!";
        return;
    }
    if (!db.isOpen()) {
        if (!db.open()) {
            qWarning() << "[upDateTableFileRecord] Failed to open DB:"
                       << db.lastError().text();
            return;
        }
    }

    // ===== helper เดิม เอามาใช้ซ้ำแบบย่อ =====
    auto bytesToHuman = [](qulonglong bytes) -> QString {
        static const char *suffixes[] = {"B","KB","MB","GB","TB","PB"};
        int i = 0;
        double cnt = static_cast<double>(bytes);
        while (cnt >= 1024.0 && i < 5) {
            cnt /= 1024.0;
            ++i;
        }
        if (i == 0)
            return QString::number(static_cast<qulonglong>(cnt)) + " " + suffixes[i];
        return QString::number(cnt, 'f',
                               (cnt < 10.0 ? 2 : (cnt < 100.0 ? 1 : 0))) +
               " " + suffixes[i];
    };

    auto fastWavDurationSec = [](const QString &path) -> double {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return -1.0;

        QByteArray hdr = file.read(12);
        if (hdr.size() < 12)
            return -1.0;

        if (memcmp(hdr.constData(), "RIFF", 4) != 0)
            return -1.0;
        if (memcmp(hdr.constData() + 8, "WAVE", 4) != 0)
            return -1.0;

        auto le16 = [](const unsigned char *p) -> quint16 {
            return quint16(p[0]) | (quint16(p[1]) << 8);
        };
        auto le32 = [](const unsigned char *p) -> quint32 {
            return quint32(p[0])
                 | (quint32(p[1]) << 8)
                 | (quint32(p[2]) << 16)
                 | (quint32(p[3]) << 24);
        };

        bool    haveFmt       = false;
        bool    haveData      = false;
        quint16 audioFormat   = 0;
        quint16 numChannels   = 0;
        quint32 sampleRate    = 0;
        quint16 bitsPerSample = 0;
        quint32 dataSize      = 0;

        while (!file.atEnd()) {
            QByteArray chHdr = file.read(8);
            if (chHdr.size() < 8)
                break;

            const unsigned char *ch = reinterpret_cast<const unsigned char*>(chHdr.constData());
            char id[5] = { (char)ch[0], (char)ch[1], (char)ch[2], (char)ch[3], 0 };
            quint32 chunkSize = le32(ch + 4);

            if (chunkSize > 1000000000u)
                break;

            if (strcmp(id, "fmt ") == 0) {
                quint32 need = qMin(chunkSize, (quint32)32);
                QByteArray fmtData = file.read(need);
                if ((quint32)fmtData.size() < need)
                    break;

                const unsigned char *p = reinterpret_cast<const unsigned char*>(fmtData.constData());
                if (fmtData.size() >= 16) {
                    audioFormat   = le16(p + 0);
                    numChannels   = le16(p + 2);
                    sampleRate    = le32(p + 4);
                    bitsPerSample = le16(p + 14);
                    haveFmt       = true;
                }

                qint64 remain = (qint64)chunkSize - (qint64)fmtData.size();
                if (remain > 0)
                    file.seek(file.pos() + remain);
            }
            else if (strcmp(id, "data") == 0) {
                dataSize = chunkSize;
                haveData = true;
                file.seek(file.pos() + chunkSize);
            }
            else {
                file.seek(file.pos() + chunkSize);
            }

            if (haveFmt && haveData)
                break;
        }

        if (!haveFmt || !haveData)
            return -1.0;
        if (sampleRate == 0 || bitsPerSample == 0 || numChannels == 0)
            return -1.0;

        quint32 bytesPerFrame = (bitsPerSample / 8u) * numChannels;
        if (bytesPerFrame == 0)
            return -1.0;

        double totalFrames = double(dataSize) / double(bytesPerFrame);
        double durationSec = totalFrames / double(sampleRate);
        if (durationSec < 0.0)
            return -1.0;

        return durationSec;
    };

    auto ffprobeDurationSec = [](const QString &path) -> double {
        QProcess proc;
        QStringList args;
        args << "-v" << "error"
             << "-show_entries" << "format=duration"
             << "-of" << "default=noprint_wrappers=1:nokey=1"
             << path;

        proc.start("ffprobe", args);
        if (!proc.waitForFinished(2000) ||
            proc.exitStatus() != QProcess::NormalExit ||
            proc.exitCode()  != 0) {
            qWarning() << "[ffprobeDurationSec] ffprobe failed for" << path
                       << "err:" << proc.readAllStandardError();
            return -1.0;
        }

        QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        bool ok = false;
        double d = out.toDouble(&ok);
        if (!ok) {
            qWarning() << "[ffprobeDurationSec] parse failed for" << path
                       << "out=" << out;
            return -1.0;
        }
        return d;
    };

    // ----- ดึง "ไฟล์ที่ใหม่กว่า" m_lastRecordCreatedAt ทั้งหมด -----
    static const char *sql =
        "SELECT "
        "  record_files.id, "
        "  record_files.device, "
        "  record_files.filename, "
        "  record_files.created_at, "
        "  record_files.continuous_count, "
        "  device_station.storage_path, "
        "  device_station.name "
        "FROM record_files "
        "JOIN device_station ON record_files.device = device_station.id "
        "WHERE record_files.created_at > :lastCreated "
        "ORDER BY record_files.created_at ASC";

    QSqlQuery q(db);
    if (!q.prepare(sql)) {
        qWarning() << "[upDateTableFileRecord] Prepare failed:"
                   << q.lastError().text();
        return;
    }
    q.bindValue(":lastCreated", m_lastRecordCreatedAt);

    if (!q.exec()) {
        qWarning() << "[upDateTableFileRecord] Query failed:"
                   << q.lastError().text();
        return;
    }

    QJsonArray recordsArray;
    bool hasNew = false;

    QString newestCreatedAt = m_lastRecordCreatedAt;
    QString newestId        = m_lastRecordId;

    while (q.next()) {
        hasNew = true;

        const QString id          = q.value("id").toString();
        const QString createdAt   = q.value("created_at").toString();
        const QString storagePath = q.value("storage_path").toString().trimmed();
        const QString filename    = q.value("filename").toString().trimmed();

        // update baseline candidate (เพราะ ORDER BY ASC ชุดนี้ ตัวสุดท้ายคือใหม่สุด)
        newestCreatedAt = createdAt;
        newestId        = id;

        const QString noExt      = filename.section('.', 0, 0);
        const QStringList parts  = noExt.split('_');
        const QString deviceName = (parts.size() >= 1) ? parts[0] : "";
        const QString date       = (parts.size() >= 2) ? parts[1] : "";

        const QString realPath =
            QString("%1/%2/%3/%4").arg(storagePath, deviceName, date, filename);

        QFileInfo fi(realPath);
        const bool exists = fi.exists() && fi.isFile();

        qulonglong sizeBytes      = 0;
        double     durationSecNum = 0.0;
        QString    humanSize;
        QString    durationStr;

        if (exists) {
            sizeBytes = static_cast<qulonglong>(fi.size());
            humanSize = bytesToHuman(sizeBytes);

            if (filename.endsWith(".wav", Qt::CaseInsensitive)) {
                double dur = fastWavDurationSec(realPath);
                if (dur < 0.0)
                    dur = ffprobeDurationSec(realPath);
                if (dur >= 0.0) {
                    dur = dur / 2.0;  // ตาม logic เดิมของคุณ
                    durationSecNum = dur;
                    durationStr    = QString::number(dur, 'f', 3);
                }
            }
        } else {
            qDebug() << "[upDateTableFileRecord] file not found:" << realPath;
        }

        QJsonObject rec;
        rec["id"]               = id;
        rec["device"]           = q.value("device").toString();
        rec["filename"]         = filename;
        rec["created_at"]       = createdAt;
        rec["continuous_count"] = q.value("continuous_count").toInt();
        rec["file_path"]        = storagePath;
        rec["full_path"]        = realPath;
        rec["name"]             = q.value("name").toString();
        rec["parsed_date"]      = date;
        rec["file_exists"]      = exists;

        if (exists) {
            rec["size_bytes"]   = static_cast<double>(sizeBytes);
            rec["size_human"]   = humanSize;
            rec["duration_sec"] = durationSecNum;
            rec["duration_str"] = durationStr;
        }

        recordsArray.append(rec);

        qDebug() << "[upDateTableFileRecord] new row:"
                 << "id="            << id
                 << "created_at="    << createdAt
                 << "exists="        << exists
                 << "realPath="      << realPath
                 << "sizeBytes="     << sizeBytes
                 << "duration_sec="  << durationSecNum;
    }

    if (!hasNew) {
        qDebug() << "[upDateTableFileRecord] no new records after"
                 << m_lastRecordCreatedAt;
        return;
    }

    // อัปเดต baseline ให้กลายเป็นตัวล่าสุดสุดของ batch นี้
    m_lastRecordCreatedAt = newestCreatedAt;
    m_lastRecordId        = newestId;

    qDebug() << "[upDateTableFileRecord] update baseline to:"
             << "created_at =" << m_lastRecordCreatedAt
             << "id ="         << m_lastRecordId;

    // ส่งออกไปให้ฝั่ง Web/QML เหมือน fetchAllRecordFiles แต่เปลี่ยน objectName
    QJsonObject result;
    result["menuID"] = "recordFilesUpdate"; // หรือ "recordFilesChunk" ก็ได้ ถ้า frontend reuse เดิม
    result["records"]    = recordsArray;
    result["isLast"]     = true;
    result["page"]       = 1;
    result["totalPages"] = 1;

    const QString message =
        QJsonDocument(result).toJson(QJsonDocument::Compact);
    qDebug() << "[upDateTableFileRecord] sent" << message;
    emit commandMysqlToCpp(message);
}


//void DatabaseiRec::searchRecordFilesMysql(QString msg, QWebSocket* wClient)
//{
//    qDebug() << "[searchRecordFilesMysql] payload:" << msg;
//    // -------- Parse input --------
//    const QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
//    const QJsonObject in = doc.object();

//    // ยอมรับทั้ง objectName และ menuID
//    const QString tag = in.value("objectName").toString(in.value("menuID").toString());
//    if (tag != "searchRecordFiles") {
//        qWarning() << "searchRecordFiles: wrong tag:" << tag;
//        return;
//    }

//    const QString deviceStr  = in.value("device").toString();     // "2"
//    const QString startStr   = in.value("startDate").toString();  // เช่น "09/22/2025, 14:00:39"
//    const QString endStr     = in.value("endDate").toString();    // เช่น "09/22/2025, 14:05:39"
//    const QString frequency  = in.value("frequency").toString();  // optional

//    const int page     = std::max(1, in.value("page").toInt(1));
//    const int pageSize = std::max(1, in.value("pageSize").toInt(25));
//    const int offset   = (page - 1) * pageSize;

//    // -------- Open DB --------
//    if (!db.isValid()) { qWarning() << "DB invalid"; return; }
//    if (!db.isOpen() && !db.open()) {
//        qWarning() << "DB open failed:" << db.lastError().text();
//        return;
//    }

//    // -------- Parse datetime แบบยืดหยุ่น --------
//    QJsonObject obj = doc.object();       // ของเดิม


//    QDateTime startDT, endDT;
//    // start
//    startDT = QDateTime::fromString(startStr, Qt::ISODate);
//    if (!startDT.isValid()) startDT = QDateTime::fromString(startStr, "MM/dd/yyyy, HH:mm:ss");
//    if (!startDT.isValid()) startDT = QDateTime::fromString(startStr, "MM/dd/yyyy HH:mm:ss");
//    if (!startDT.isValid()) {
//        QLocale us(QLocale::English, QLocale::UnitedStates);
//        startDT = us.toDateTime(startStr, "MM/dd/yyyy, hh:mm:ss AP");
//        if (!startDT.isValid())
//            startDT = us.toDateTime(startStr, "MM/dd/yyyy hh:mm:ss AP");
//    }
//    if (!startDT.isValid()) startDT = QDateTime::fromString(startStr, "yyyy/MM/dd HH:mm:ss");

//    // end
//    endDT = QDateTime::fromString(endStr, Qt::ISODate);
//    if (!endDT.isValid()) endDT = QDateTime::fromString(endStr, "MM/dd/yyyy, HH:mm:ss");
//    if (!endDT.isValid()) endDT = QDateTime::fromString(endStr, "MM/dd/yyyy HH:mm:ss");
//    if (!endDT.isValid()) {
//        QLocale us(QLocale::English, QLocale::UnitedStates);
//        endDT = us.toDateTime(endStr, "MM/dd/yyyy, hh:mm:ss AP");
//        if (!endDT.isValid())
//            endDT = us.toDateTime(endStr, "MM/dd/yyyy hh:mm:ss AP");
//    }
//    if (!endDT.isValid()) endDT = QDateTime::fromString(endStr, "yyyy/MM/dd HH:mm:ss");

//    if (!startDT.isValid() || !endDT.isValid()) {
//        qWarning() << "Invalid start/end date format. start=" << startStr << " end=" << endStr;
//        return;
//    }

//    startDT.setTimeSpec(Qt::LocalTime);
//    endDT.setTimeSpec(Qt::LocalTime);

//    const QString mysqlStart = startDT.toString("yyyy-MM-dd HH:mm:ss");
//    const QString mysqlEnd   = endDT.toString("yyyy-MM-dd HH:mm:ss");

//    // -------- COUNT (สำหรับ totalPages) --------
//    int totalRows = 0;
//    {
//        QString countSQL =
//            "SELECT COUNT(*) "
//            "FROM record_files "
//            "WHERE device = :device "
//            "  AND created_at BETWEEN :start AND :end ";

//        bool fOK = false;
//        const double f = frequency.toDouble(&fOK);
//        if (!frequency.isEmpty()) {
//            countSQL += " AND ( filename LIKE :pat1 OR filename LIKE :pat2 ";
//            if (fOK)  countSQL += " OR filename LIKE :pat3 ";
//            countSQL += ") ";
//        }

//        QSqlQuery cq(db);
//        if (!cq.prepare(countSQL)) { qWarning() << "count prepare:" << cq.lastError().text(); return; }
//        cq.bindValue(":device", deviceStr.toInt());
//        cq.bindValue(":start",  mysqlStart);
//        cq.bindValue(":end",    mysqlEnd);

//        if (!frequency.isEmpty()) {
//            cq.bindValue(":pat1", "%" + frequency + ".wav");   // …_121.950.wav
//            cq.bindValue(":pat2", "%_" + frequency + "_%");
//            if (fOK) {
//                const int kHzTimes1000 = static_cast<int>(f * 1000.0 + 0.5);
//                cq.bindValue(":pat3", QString::number(kHzTimes1000) + "_%");
//            }
//        }

//        if (!cq.exec()) { qWarning() << "count exec:" << cq.lastError().text(); return; }
//        if (cq.next()) totalRows = cq.value(0).toInt();
//    }
//    const int totalPages = (totalRows + pageSize - 1) / pageSize;

//    // -------- FETCH (หน้า page) --------
//    QString sql =
//        "SELECT id, device, filename, created_at, continuous_count, file_path, name "
//        "FROM record_files "
//        "WHERE device = :device "
//        "  AND created_at BETWEEN :start AND :end ";

//    bool fOK = false; double f = frequency.toDouble(&fOK);
//    if (!frequency.isEmpty()) {
//        sql += " AND ( filename LIKE :pat1 OR filename LIKE :pat2 ";
//        if (fOK) sql += " OR filename LIKE :pat3 ";
//        sql += ") ";
//    }

//    sql += "ORDER BY created_at DESC "
//           "LIMIT :limit OFFSET :offset";

//    QSqlQuery q(db);
//    if (!q.prepare(sql)) { qWarning() << "prepare:" << q.lastError().text(); return; }
//    q.bindValue(":device", deviceStr.toInt());
//    q.bindValue(":start",  mysqlStart);
//    q.bindValue(":end",    mysqlEnd);
//    if (!frequency.isEmpty()) {
//        q.bindValue(":pat1", "%" + frequency + ".wav");
//        q.bindValue(":pat2", "%_" + frequency + "_%");
//        if (fOK) {
//            const int kHzTimes1000 = static_cast<int>(f * 1000.0 + 0.5);
//            q.bindValue(":pat3", QString::number(kHzTimes1000) + "_%");
//        }
//    }
//    q.bindValue(":limit",  pageSize);
//    q.bindValue(":offset", offset);

//    if (!q.exec()) { qWarning() << "exec:" << q.lastError().text(); return; }

//    // ---------- helper: คำนวณ duration ด้วย ffprobe ----------
//    auto ffprobeDurationSec = [](const QString &path) -> double {
//        QProcess proc;
//        QStringList args;
//        args << "-v" << "error"
//             << "-show_entries" << "format=duration"
//             << "-of" << "default=noprint_wrappers=1:nokey=1"
//             << path;

//        proc.start("ffprobe", args);
//        if (!proc.waitForFinished(2000) ||
//            proc.exitStatus() != QProcess::NormalExit ||
//            proc.exitCode()  != 0) {
//            qWarning() << "[ffprobeDurationSec] ffprobe failed for" << path
//                       << "err:" << proc.readAllStandardError();
//            return -1.0;
//        }

//        QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
//        bool ok = false;
//        double d = out.toDouble(&ok);
//        if (!ok) {
//            qWarning() << "[ffprobeDurationSec] parse failed for" << path << "out=" << out;
//            return -1.0;
//        }
//        return d;
//    };

//    // -------- Pack JSON --------
//    QJsonArray rows;
//    while (q.next()) {
//        const QString filename = q.value("filename").toString();
//        const QString filePath = q.value("file_path").toString(); // "/var/ivoicex"

//        QJsonObject r;
//        r["id"]               = q.value("id").toString();
//        r["device"]           = q.value("device").toString();
//        r["filename"]         = filename;
//        r["created_at"]       = q.value("created_at").toString();
//        r["continuous_count"] = q.value("continuous_count").toInt();
//        r["file_path"]        = filePath;
//        r["name"]             = q.value("name").toString();

//        // parsed_date จาก filename: "<deviceName>_<YYYYMMDD>_....wav"
//        const QString fnameNoExt = filename.section('.', 0, 0);
//        const QStringList parts  = fnameNoExt.split('_');
//        QString deviceName, ymd;
//        if (parts.size() >= 2) {
//            deviceName = parts[0];   // "23-iGate23-4"
//            ymd        = parts[1];   // "20250922"
//            r["parsed_date"] = ymd;
//        }

//        // ประกอบ full path: /var/ivoicex/<deviceName>/<YYYYMMDD>/<filename>
//        QString fullPath = QDir::cleanPath(filePath + "/" + deviceName + "/" + ymd + "/" + filename);
//        r["full_path"] = fullPath;

//        QFileInfo fi(fullPath);
//        double sizeBytes  = -1.0;
//        double sizeKB     = -1.0;
//        double durSec     = -1.0;
//        QString sizeStr;
//        QString durStr;

//        if (fi.exists() && fi.isFile()) {
//            sizeBytes = static_cast<double>(fi.size());
//            sizeKB    = sizeBytes / 1024.0;
//            sizeStr   = QString::number(sizeKB, 'f', 3);  // เช่น "66.293"

//            if (filename.endsWith(".wav", Qt::CaseInsensitive)) {
//                durSec = ffprobeDurationSec(fullPath);    // ใช้ ffprobe
//                if (durSec >= 0.0) {
//                    durStr = QString::number(durSec, 'f', 3);
//                }
//            }
//        }


//        // ส่งออกไปให้ QML
//        if (sizeBytes >= 0.0) {
//            r["size_bytes"] = sizeBytes;   // numeric ใช้ sum ใน QML ได้
//            r["size"]       = sizeStr;     // string สวย ๆ
//        } else {
//            r["size_bytes"] = 0.0;
//            r["size"]       = "";
//        }

//        if (durSec >= 0.0) {
//            r["duration_sec"] = durSec;    // numeric
//            r["duration_str"] = durStr;    // string
//        } else {
//            r["duration_sec"] = 0.0;
//            r["duration_str"] = "";
//        }

//        // debug ช่วยตรวจสอบ path/ผลลัพธ์
//        qDebug() << "[rec]" << fullPath
//                 << "exists=" << fi.exists()
//                 << "sizeBytes=" << sizeBytes
//                 << "durSec=" << durSec;

//        rows.append(r);
//    }

//    // -------- Reply --------
//    QJsonObject out;
//    out["objectName"] = "recordFilesChunk";
//    out["records"]    = rows;
//    out["page"]       = page;
//    out["totalPages"] = totalPages;
//    out["isLast"]     = (page >= totalPages);

//    const QString payload = QJsonDocument(out).toJson(QJsonDocument::Compact);

//    emit commandMysqlToCpp(payload);
//    // if (wClient && wClient->isValid()) { wClient->sendTextMessage(payload); }

//    qDebug() << "[searchRecordFilesMysql] page:" << page
//             << "rows:" << rows.size()
//             << "totalPages:" << totalPages
//             << "range:" << mysqlStart << "->" << mysqlEnd;
//    {
//        QJsonObject statusDone;
//        statusDone["menuID"] = "statusSearchFiles";
//        statusDone["status"] = "Done";
//        QString statusJsonDone = QJsonDocument(statusDone).toJson(QJsonDocument::Compact);
//        qDebug() << "statusJsonDone:" << statusJsonDone;
//        emit commandMysqlToCpp(statusJsonDone);
//    }
//}

void DatabaseiRec::filterRecordFiles(QString msg, QWebSocket* wClient)
{
    qDebug() << "filterRecordFilesMysql:" << msg;

    // -------- Parse JSON input --------
    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON format";
        return;
    }
    QJsonObject obj = doc.object();

    QString date      = obj.value("dateTime").toString().trimmed();
    QString startTime = obj.value("startTime").toString().trimmed();
    QString endTime   = obj.value("endTime").toString().trimmed();

    if (date.isEmpty()) {
        qWarning() << "Missing date in JSON";
        return;
    }

    // validate time format if both provided
    if (!startTime.isEmpty() && !endTime.isEmpty()) {
        QTime tStart = QTime::fromString(startTime, "HH:mm:ss");
        QTime tEnd   = QTime::fromString(endTime,   "HH:mm:ss");
        if (!tStart.isValid() || !tEnd.isValid()) {
            qWarning() << "Invalid time format!";
            return;
        }
    }

    // -------- Ensure DB open --------
    if (!db.isValid()) {
        qWarning() << "Database connection is invalid!";
        return;
    }
    if (!db.isOpen()) {
        if (!db.open()) {
            qWarning() << "Failed to open DB:" << db.lastError().text();
            return;
        }
    }

    // -------- Build SQL --------
    QString sql = R"(
        SELECT id, device, filename, created_at,
               continuous_count, file_path, name
        FROM record_files
        WHERE DATE(created_at) = :date
    )";

    if (!startTime.isEmpty() && !endTime.isEmpty()) {
        sql += " AND TIME(created_at) BETWEEN :start_time AND :end_time";
    }

    QSqlQuery query(db);
    if (!query.prepare(sql)) {
        qWarning() << "Prepare failed:" << query.lastError().text();
        return;
    }

    query.bindValue(":date", date);
    if (!startTime.isEmpty() && !endTime.isEmpty()) {
        query.bindValue(":start_time", startTime);
        query.bindValue(":end_time",   endTime);
    }

    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        return;
    }

    // ===== FAST WAV DURATION (อ่าน header เอง) =====
    auto fastWavDurationSec = [](const QString &path) -> double {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return -1.0;

        // RIFF header 12 bytes: "RIFF" + fileSize + "WAVE"
        QByteArray hdr = file.read(12);
        if (hdr.size() < 12)
            return -1.0;

        if (memcmp(hdr.constData(), "RIFF", 4) != 0)
            return -1.0;
        if (memcmp(hdr.constData() + 8, "WAVE", 4) != 0)
            return -1.0;

        auto le16 = [](const unsigned char *p) -> quint16 {
            return quint16(p[0]) | (quint16(p[1]) << 8);
        };
        auto le32 = [](const unsigned char *p) -> quint32 {
            return quint32(p[0])
                 | (quint32(p[1]) << 8)
                 | (quint32(p[2]) << 16)
                 | (quint32(p[3]) << 24);
        };

        bool    haveFmt       = false;
        bool    haveData      = false;
        quint16 audioFormat   = 0;
        quint16 numChannels   = 0;
        quint32 sampleRate    = 0;
        quint16 bitsPerSample = 0;
        quint32 dataSize      = 0;

        while (!file.atEnd()) {
            QByteArray chHdr = file.read(8);
            if (chHdr.size() < 8)
                break;

            const unsigned char *ch = reinterpret_cast<const unsigned char*>(chHdr.constData());
            char id[5] = { (char)ch[0], (char)ch[1], (char)ch[2], (char)ch[3], 0 };
            quint32 chunkSize = le32(ch + 4);

            // กันค่าเพี้ยน ๆ
            if (chunkSize > 1000000000u)
                break;

            if (strcmp(id, "fmt ") == 0) {
                quint32 need = qMin(chunkSize, (quint32)32);
                QByteArray fmtData = file.read(need);
                if ((quint32)fmtData.size() < need)
                    break;

                const unsigned char *p = reinterpret_cast<const unsigned char*>(fmtData.constData());
                if (fmtData.size() >= 16) {
                    audioFormat   = le16(p + 0);   // 1 = PCM
                    numChannels   = le16(p + 2);
                    sampleRate    = le32(p + 4);
                    bitsPerSample = le16(p + 14);
                    haveFmt       = true;
                }

                qint64 remain = (qint64)chunkSize - (qint64)fmtData.size();
                if (remain > 0)
                    file.seek(file.pos() + remain);
            }
            else if (strcmp(id, "data") == 0) {
                dataSize = chunkSize;
                haveData = true;
                file.seek(file.pos() + chunkSize);  // skip real data
            }
            else {
                // chunk อื่น ๆ ข้ามไป
                file.seek(file.pos() + chunkSize);
            }

            if (haveFmt && haveData)
                break;
        }

        if (!haveFmt || !haveData)
            return -1.0;
        if (sampleRate == 0 || bitsPerSample == 0 || numChannels == 0)
            return -1.0;

        quint32 bytesPerFrame = (bitsPerSample / 8u) * numChannels;
        if (bytesPerFrame == 0)
            return -1.0;

        double totalFrames = double(dataSize) / double(bytesPerFrame);
        double durationSec = totalFrames / double(sampleRate);

        if (durationSec < 0.0)
            return -1.0;

        return durationSec;
    };

    // ===== SLOW FALLBACK (ffprobe) =====
    auto ffprobeDurationSec = [](const QString &path) -> double {
        QProcess proc;
        QStringList args;
        args << "-v" << "error"
             << "-show_entries" << "format=duration"
             << "-of" << "default=noprint_wrappers=1:nokey=1"
             << path;

        proc.start("ffprobe", args);
        if (!proc.waitForFinished(2000) ||
            proc.exitStatus() != QProcess::NormalExit ||
            proc.exitCode()  != 0) {
            qWarning() << "[ffprobeDurationSec] ffprobe failed for" << path
                       << "err:" << proc.readAllStandardError();
            return -1.0;
        }

        QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        bool ok = false;
        double d = out.toDouble(&ok);
        if (!ok) {
            qWarning() << "[ffprobeDurationSec] parse failed for" << path << "out=" << out;
            return -1.0;
        }
        return d;
    };

    // -------- Pack JSON result --------
    QJsonArray resultArray;
    int rowsCount = 0;

    while (query.next()) {
        QJsonObject record;

        record["id"]               = QString(query.value("id").toByteArray().toHex());
        record["device"]           = query.value("device").toString();
        record["filename"]         = query.value("filename").toString();
        record["created_at"]       = query.value("created_at").toString();
        record["continuous_count"] = query.value("continuous_count").toInt();
        record["file_path"]        = query.value("file_path").toString();
        record["name"]             = query.value("name").toString();

        const QString filename = record["filename"].toString();
        const QString filePath = record["file_path"].toString();

        // ==== parsed_date / full_path แบบเดียวกับ searchRecordFilesMysql ====
        const QString fnameNoExt = filename.section('.', 0, 0);
        const QStringList parts  = fnameNoExt.split('_');
        QString deviceName, ymd;
        if (parts.size() >= 2) {
            deviceName = parts[0];   // เช่น "23-iGate23-4"
            ymd        = parts[1];   // เช่น "20250922"
            record["parsed_date"] = ymd;
        }

        QString fullPath = QDir::cleanPath(filePath + "/" + deviceName + "/" + ymd + "/" + filename);
        record["full_path"] = fullPath;

        // ==== STAT ไฟล์ + duration ====
        QFileInfo fi(fullPath);
        double sizeBytes  = -1.0;
        double sizeKB     = -1.0;
        double durSec     = -1.0;
        QString sizeStr;
        QString durStr;

        if (fi.exists() && fi.isFile()) {
            sizeBytes = static_cast<double>(fi.size());
            sizeKB    = sizeBytes / 1024.0;
            sizeStr   = QString::number(sizeKB, 'f', 3);

            if (filename.endsWith(".wav", Qt::CaseInsensitive)) {
                // 1) ใช้ fast header parser ก่อน (เร็ว)
                durSec = fastWavDurationSec(fullPath);

                // 2) ถ้าพัง → ใช้ ffprobe (ช้าแต่ชัวร์)
                if (durSec < 0.0)
                    durSec = ffprobeDurationSec(fullPath);

                // ★★ หาร 2 ตามที่บอก เพราะค่าที่ได้มามัน 2x ★★
                if (durSec >= 0.0) {
                    durSec = durSec / 2.0;
                    durStr = QString::number(durSec, 'f', 3);
                }
            }
        }

        if (sizeBytes >= 0.0) {
            record["size_bytes"] = sizeBytes;
            record["size"]       = sizeStr;
        } else {
            record["size_bytes"] = 0.0;
            record["size"]       = "";
        }

        if (durSec >= 0.0) {
            record["duration_sec"] = durSec;
            record["duration_str"] = durStr;
        } else {
            record["duration_sec"] = 0.0;
            record["duration_str"] = "";
        }

        qDebug() << "[filter rec]" << fullPath
                 << "exists=" << fi.exists()
                 << "sizeBytes=" << sizeBytes
                 << "durSec=" << durSec;

        resultArray.append(record);
        ++rowsCount;
    }

    // -------- Build response --------
    QJsonObject response;
    response["objectName"] = "filteredRecordFiles";
    response["totalRows"]  = rowsCount;
    response["records"]    = resultArray;

    QJsonDocument responseDoc(response);
    QString messageOut = responseDoc.toJson(QJsonDocument::Compact);

    if (wClient && wClient->isValid()) {
        wClient->sendTextMessage(messageOut);
        qDebug() << "Sent filtered records:" << messageOut;
    }

    qDebug() << "[filterRecordFiles] date=" << date
             << "rows=" << rowsCount
             << "startTime=" << startTime
             << "endTime=" << endTime;
}

//void DatabaseiRec::filterRecordFiles(QString msg, QWebSocket* wClient)
//{
//    qDebug() << "filterRecordFilesMysql:" << msg;

//    // Parse JSON
//    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
//    if (!doc.isObject())
//    {
//        qWarning() << "Invalid JSON format";
//        return;
//    }
//    QJsonObject obj = doc.object();

//    QString date = obj["dateTime"].toString().trimmed();
//    QString startTime = obj["startTime"].toString().trimmed();
//    QString endTime = obj["endTime"].toString().trimmed();

//    if (date.isEmpty())
//    {
//        qWarning() << "Missing date in JSON";
//        return;
//    }

//    // Validate times if provided
//    if (!startTime.isEmpty() && !endTime.isEmpty()) {
//        QTime tStart = QTime::fromString(startTime, "HH:mm:ss");
//        QTime tEnd = QTime::fromString(endTime, "HH:mm:ss");
//        if (!tStart.isValid() || !tEnd.isValid()) {
//            qWarning() << "Invalid time format!";
//            return;
//        }
//    }

//    if (!db.isValid()) {
//        qWarning() << "Database connection is invalid!";
//        return;
//    }
//    if (!db.isOpen()) {
//        if (!db.open()) {
//            qWarning() << "Failed to open DB:" << db.lastError().text();
//            return;
//        }
//    }

//    QString sql = R"(
//        SELECT *
//        FROM record_files
//        WHERE DATE(created_at) = :date
//    )";

//    if (!startTime.isEmpty() && !endTime.isEmpty()) {
//        sql += " AND TIME(created_at) BETWEEN :start_time AND :end_time";
//    }

//    QSqlQuery query(db);
//    if (!query.prepare(sql))
//    {
//        qWarning() << "Prepare failed:" << query.lastError().text();
//        return;
//    }

//    query.bindValue(":date", date);
//    if (!startTime.isEmpty() && !endTime.isEmpty()) {
//        query.bindValue(":start_time", startTime);
//        query.bindValue(":end_time", endTime);
//    }

//    if (!query.exec())
//    {
//        qWarning() << "Query failed:" << query.lastError().text();
//        return;
//    }

//    QJsonArray resultArray;
//    int rowsCount = 0;

//    while (query.next())
//    {
//        QJsonObject record;
//        record["id"] = QString(query.value("id").toByteArray().toHex());
//        record["device"] = query.value("device").toString();
//        record["filename"] = query.value("filename").toString();
//        record["created_at"] = query.value("created_at").toString();
//        record["continuous_count"] = query.value("continuous_count").toInt();
//        record["file_path"] = query.value("file_path").toString();
//        record["name"] = query.value("name").toString();

//        resultArray.append(record);
//        rowsCount++;
//    }

//    QJsonObject response;
//    response["objectName"] = "filteredRecordFiles";
//    response["totalRows"] = rowsCount;
//    response["records"] = resultArray;

//    QJsonDocument responseDoc(response);
//    QString messageOut = responseDoc.toJson(QJsonDocument::Compact);

//    if (wClient && wClient->isValid())
//    {
//        wClient->sendTextMessage(messageOut);
//        qDebug() << "Sent filtered records:" << messageOut;
//    }
//}


QString DatabaseiRec::generateBcryptHash(const QString& password)
{
    QString cmd = QString("htpasswd -nbBC 12 dummy \"%1\"").arg(password);
    QProcess proc;
    proc.start(cmd);
    proc.waitForFinished();
    QByteArray result = proc.readAllStandardOutput();

    QString out(result);
    QStringList parts = out.split(":");
    if (parts.size() == 2) {
        QString bcryptHash = parts[1].trimmed();
        return bcryptHash;
    } else {
        qDebug() << "Cannot generate bcrypt hash. Output:" << out;
        return QString();
    }
}

void DatabaseiRec::addNewUsers(QString msg, QWebSocket* wClient)
{
    qDebug() << "addNewUsers called" << msg;

    if (!db.isOpen()) {
        if (!db.open()) {
            qWarning() << "Cannot connect to database:" << db.lastError().text();
            return;
        }
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "JSON parse error:" << parseError.errorString();
        return;
    }

    QJsonObject obj = doc.object();

    QString username = obj["username"].toString();
    QString password = obj["password"].toString();
    int userlevel = obj["userlevel"].toInt();

    if (username.isEmpty() || password.isEmpty()) {
        qDebug() << "Invalid data for user creation.";
        return;
    }

    int sipPort = 5060;
    int rtpStartPort = 5002;
    int keepAlivePeroid = 200;
    int clockrate = 8000;
    QString sipUser = username;
    int enableRecoreder = 1;

    int userID = (userlevel == 1) ? 1 : 2;

    // MySQL password hash
    QByteArray sha1_first = QCryptographicHash::hash(
        password.toUtf8(),
        QCryptographicHash::Sha1
    );

    QByteArray sha1_second = QCryptographicHash::hash(
        sha1_first,
        QCryptographicHash::Sha1
    );

    QString mysqlPassword = "*" + sha1_second.toHex().toUpper();
    qDebug() << "MySQL password hash:" << mysqlPassword;

    // webpassword (bcrypt)
    QString webpassword = generateBcryptHash(password);
    if (webpassword.isEmpty()) {
        webpassword = "$2y$12$XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    }
    qDebug() << "bcrypt webpassword:" << webpassword;

    QString sql = R"(
        INSERT INTO controler
            (sipPort, rtpStartPort, keepAlivePeroid, clockrate,
             sipUser, userID, username, password,
             userlevel, webpassword, enableRecoreder)
        VALUES
            (:sipPort, :rtpStartPort, :keepAlivePeroid, :clockrate,
             :sipUser, :userID, :username, :password,
             :userlevel, :webpassword, :enableRecoreder)
    )";

    QSqlQuery query(db);
    query.prepare(sql);
    query.bindValue(":sipPort", sipPort);
    query.bindValue(":rtpStartPort", rtpStartPort);
    query.bindValue(":keepAlivePeroid", keepAlivePeroid);
    query.bindValue(":clockrate", clockrate);
    query.bindValue(":sipUser", sipUser);
    query.bindValue(":userID", userID);
    query.bindValue(":username", username);
    query.bindValue(":password", mysqlPassword);
    query.bindValue(":userlevel", userlevel);
    query.bindValue(":webpassword", webpassword);
    query.bindValue(":enableRecoreder", enableRecoreder);

    if (!query.exec()) {
        qWarning() << "Insert failed:" << query.lastError().text();
        return;
    }

    qDebug() << "Inserted new user into controler:" << username;

    QJsonObject response;
    response["objectName"] = "insertNewUserInDatabase";
    response["result"] = "OK";
    response["username"] = username;

    QJsonDocument responseDoc(response);
    QString jsonString = responseDoc.toJson(QJsonDocument::Compact);

    if (wClient != nullptr) {
        qDebug() << "insertNewUserInDatabase:" << jsonString;
        wClient->sendTextMessage(jsonString);
    }
}

void DatabaseiRec::selectLocalStreaming(QString ip) {
    // Static variable to remember last IP
    static QString lastQueriedIp;

    // Special command to reset last IP (e.g., after ffmpeg stop)
    if (ip == "disable") {
        lastQueriedIp.clear();
        qDebug() << "[selectLocalStreaming] Reset due to 'disable' command.";
        return;
    }

    // Avoid querying same IP repeatedly
    if (ip == lastQueriedIp) {
        qDebug() << "[selectLocalStreaming] IP already processed:" << ip;
        return;
    }

    // Update to new IP
    lastQueriedIp = ip;

    qDebug() << "selectLocalStreaming:" << ip;

    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);
    query.prepare("SELECT * FROM device_station WHERE ip = :ip LIMIT 1");
    query.bindValue(":ip", ip);

    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        return;
    }

    if (query.next()) {
        // Build JSON object in Jetson format
        QJsonObject device;
        device["id"] = query.value("id").toInt();
        device["sid"] = query.value("sid").toInt();
        device["payload_size"] = query.value("payload_size").toInt();
        device["terminal_type"] = query.value("terminal_type").toInt();
        device["name"] = query.value("name").toString();
        device["ip"] = query.value("ip").toString();
        device["uri"] = query.value("uri").toString();
        device["freq"] = query.value("freq").toInt();
        device["ambient"] = query.value("ambient").isNull() ? QJsonValue() : QJsonValue(query.value("ambient").toInt());
        device["group"] = query.value("group").isNull() ? QJsonValue() : QJsonValue(query.value("group").toInt());
        device["visible"] = query.value("visible").toBool();
        device["last_access"] = query.value("last_access").isNull() ? QJsonValue() : QJsonValue(query.value("last_access").toString());
        device["storage_path"] = query.value("storage_path").toString();
        device["chunk"] = query.value("chunk").toInt();
        device["updated_at"] = query.value("updated_at").toDateTime().toString("yyyy-MM-dd HH:mm:ss");

        QJsonObject root;
        root["menuID"] = "localStreamingInfo";
        root["device"] = device;

        QJsonDocument doc(root);
        QString jsonStr = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

        qDebug() << "[selectLocalStreaming] Packed JSON:" << jsonStr;

        emit cmddatabaseTomain(jsonStr);
    } else {
        qDebug() << "No matching device found for IP:" << ip;
    }
}

void DatabaseiRec::lookupDeviceStationByIp(const QString& megs, QWebSocket* wClient) {
    qDebug() << "lookupDeviceStationByIp:" << megs;

    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QStringList parts = megs.split(",");
    if (parts.size() != 4) {
        qWarning() << "Invalid message format";
        return;
    }

    QString ip     = parts[0].trimmed();
    QString freq   = parts[1].trimmed();
    QString url    = parts[2].trimmed();
    QString action = parts[3].trimmed();

    QSqlQuery query(db);  // ใช้ db ตัวหลัก
    query.prepare("SELECT id, name FROM device_station WHERE ip = :ip LIMIT 1");
    query.bindValue(":ip", ip);

    if (query.exec()) {
        if (query.next()) {
            int sid = query.value("id").toInt();
            QString name = query.value("name").toString();

            QJsonObject obj;
            obj["ip"] = ip;
            obj["freq"] = freq;
            obj["url"] = url;
            obj["action"] = action;
            obj["sid"] = sid;
            obj["name"] = name;

            qDebug() << "Device found:" << obj;
            QThread::msleep(100);
            recordToRecordChannel(obj);
        } else {
            qWarning() << "Device not found for IP:" << ip;
        }
    } else {
        qWarning() << "Failed to execute query:" << query.lastError().text();
    }
//    db.close();
}

void DatabaseiRec::formatDatabases(QString megs) {
    qDebug() << "Received format command:" << megs;
    QByteArray br = megs.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);
    QJsonObject obj = doc.object();
    QJsonObject command = doc.object();
    QString menuID = obj["menuID"].toString().trimmed();

    if (menuID == "formatExternal") {
        qDebug() << "formatDatabases:" << megs;
        qDebug() << "Database path:" << db.databaseName();

        if (!db.isOpen()) {
            if (!db.open()) {
                qDebug() << "❌ Failed to open database:" << db.lastError().text();
                return;
            }
        }

        QSqlQuery pragmaQuery(db);
        pragmaQuery.exec("PRAGMA foreign_keys = OFF;"); // for SQLite

        QSqlQuery query(db);
        QString cmd = "DELETE FROM record_files WHERE 1=1;";
        if (!query.exec(cmd)) {
            qDebug() << "❌ Failed to clear record_files table:" << query.lastError().text();
        } else {
            qDebug() << "✅ Cleared table record_files successfully.";
        }

        // ตรวจสอบว่าลบหมดจริงหรือไม่
        QSqlQuery countQuery("SELECT COUNT(*) FROM record_files;", db);
        if (countQuery.next()) {
            int remaining = countQuery.value(0).toInt();
            qDebug() << "Remaining rows in record_files:" << remaining;
        }

//        db.close();
    }
}


void DatabaseiRec::recordToRecordChannel(const QJsonObject& obj) {
    QString ip      = obj.value("ip").toString().trimmed();
    QString url     = obj.value("url").toString().trimmed();
    QString action  = obj.value("action").toString().trimmed();
    QString name    = obj.value("name").toString().trimmed();
    int sid         = obj.value("sid").toInt(-1);
    int freq        = 0;

    if (obj.contains("freq")) {
        if (obj["freq"].isString())
            freq = obj["freq"].toString().toInt();
        else if (obj["freq"].isDouble())
            freq = obj["freq"].toInt();
    }

    if (sid < 0) {
        qWarning() << "Invalid SID, aborting.";
        return;
    }

    if (!db.isValid() || !db.isOpen()) {
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    const QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    QSqlQuery queryCheck(db);
    queryCheck.prepare("SELECT id, sid, ip, url, name, freq FROM record_channel WHERE ip = :ip LIMIT 1");
    queryCheck.bindValue(":ip", ip);


    if (!queryCheck.exec()) {
        qWarning() << "Failed to SELECT from record_channel:" << queryCheck.lastError().text();
        return;
    }

    if (queryCheck.next()) {
        int id = queryCheck.value("id").toInt();
        QString ipDB   = queryCheck.value("ip").toString();
        QString urlDB  = queryCheck.value("url").toString();
        QString nameDB = queryCheck.value("name").toString();
        int freqDB     = queryCheck.value("freq").toInt();

        bool needsUpdate = (ipDB != ip || urlDB != url || nameDB != name || freqDB != freq);

        if (needsUpdate) {
            QSqlQuery updateQuery(db);
            updateQuery.prepare(R"(
                UPDATE record_channel
                SET ip = :ip, url = :url, name = :name, freq = :freq,
                    action = :action, updated_at = :updated
                WHERE id = :id
            )");
            updateQuery.bindValue(":ip", ip);
            updateQuery.bindValue(":url", url);
            updateQuery.bindValue(":name", name);
            updateQuery.bindValue(":freq", freq);
            updateQuery.bindValue(":action", action);
            updateQuery.bindValue(":updated", now);
            updateQuery.bindValue(":id", id);

            if (!updateQuery.exec()) {
                qWarning() << "Failed to update record_channel:" << updateQuery.lastError().text();
            } else {
                qDebug() << "Updated record_channel with SID:" << sid;
            }
        } else {
            qDebug() << "No update needed for SID:" << sid;
        }
    } else {
        QSqlQuery insertQuery(db);
        insertQuery.prepare(R"(
            INSERT INTO record_channel (sid, ip, url, action, name, freq, created_at, updated_at)
            VALUES (:sid, :ip, :url, :action, :name, :freq, :created, :updated)
        )");
        insertQuery.bindValue(":sid", sid);
        insertQuery.bindValue(":ip", ip);
        insertQuery.bindValue(":url", url);
        insertQuery.bindValue(":action", action);
        insertQuery.bindValue(":name", name);
        insertQuery.bindValue(":freq", freq);
        insertQuery.bindValue(":created", now);
        insertQuery.bindValue(":updated", now);

        if (!insertQuery.exec()) {
            qWarning() << "Failed to insert into record_channel:" << insertQuery.lastError().text();
        } else {
            qDebug() << "Inserted new record_channel with SID:" << sid;
        }
    }
}


bool DatabaseiRec::tableExists(QSqlDatabase& db, const QString& tableName)
{
    QSqlQuery q(db);
    q.prepare("SELECT 1 FROM INFORMATION_SCHEMA.TABLES "
              "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = :t LIMIT 1");
    q.bindValue(":t", tableName);
    if (!q.exec()) {
        qWarning() << "tableExists() failed:" << q.lastError().text();
        return false;
    }
    return q.next();
}

static QSet<QString> getExistingColumns(QSqlDatabase& db, const QString& tableName) {
    QSet<QString> cols;
    QSqlQuery q(db);
    q.prepare("SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
              "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = :t");
    q.bindValue(":t", tableName);
    if (!q.exec()) {
        qWarning() << "getExistingColumns() failed:" << q.lastError().text();
        return cols;
    }
    while (q.next()) {
        cols.insert(q.value(0).toString());
    }
    return cols;
}
static bool addColumnIfMissing(QSqlDatabase& db, const QString& tableName, const QString& colName, const QString& colDDL) {
    const QSet<QString> existing = getExistingColumns(db, tableName);
    if (existing.contains(colName)) {
        return true; // มีแล้ว
    }
    const QString alter = QString("ALTER TABLE `%1` ADD COLUMN %2").arg(tableName, colDDL);
    QSqlQuery q(db);
    if (!q.exec(alter)) {
        qWarning() << "Failed to ADD COLUMN" << colName << "on" << tableName << ":" << q.lastError().text();
        return false;
    }
    qDebug() << "Added missing column" << colName << "to" << tableName;
    return true;
}
void DatabaseiRec::CheckandVerifyDatabases() {
    qDebug() << "CheckAndVerifyDatabases started";

     if (!db.isOpen()) {
         qDebug() << "Opening database...";
         if (!db.open()) {
             qWarning() << "Failed to open database:" << db.lastError().text();
             return;
         }
     }

     // 1) นิยาม DDL สำหรับ "สร้างตารางใหม่" เมื่อยังไม่มี
     QMap<QString, QString> tableCreateDDL = {
         { "device_access", R"SQL(
             CREATE TABLE device_access (
                 id INT AUTO_INCREMENT PRIMARY KEY,
                 port_src VARCHAR(30),
                 ip_src   VARCHAR(30),
                 uri      VARCHAR(30),
                 device   INT UNSIGNED,
                 active   TINYINT(1)
             )
         )SQL" },

         { "device_group", R"SQL(
             CREATE TABLE device_group (
                 id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
                 name       VARCHAR(255),
                 lastupdate DATETIME,
                 visible    TINYINT(1) NOT NULL DEFAULT 1
             )
         )SQL" },

         // NOTE: เพิ่ม storage_path ด้วย (ของเดิมคุณยังไม่มีใน CREATE)
         { "device_station", R"SQL(
             CREATE TABLE device_station (
                 id            INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
                 sid           INT UNSIGNED UNIQUE,
                 payload_size  INT,
                 terminal_type INT,
                 name          VARCHAR(255),
                 ip            VARCHAR(255),
                 uri           VARCHAR(255),
                 freq          DECIMAL(10,0),
                 ambient       TINYINT(1) DEFAULT 0,
                 `group`       INT UNSIGNED,
                 visible       TINYINT(1) DEFAULT 0,
                 last_access   DATETIME,
                 storage_path  VARCHAR(512),
                 chunk         INT DEFAULT 0,
                 updated_at    TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
             )
         )SQL" },

         { "devices", R"SQL(
             CREATE TABLE devices (
                 id INT AUTO_INCREMENT PRIMARY KEY,
                 deviceId      VARCHAR(255),
                 frequency     VARCHAR(255),
                 mode          VARCHAR(50),
                 ip            VARCHAR(50),
                 timeInterval  INT,
                 pathDirectory VARCHAR(255),
                 companyName   VARCHAR(255)
             )
         )SQL" },

         // NOTE: เพิ่ม file_path, name ให้ตรงกับของจริงในฐาน
         { "record_channel", R"SQL(
             CREATE TABLE record_channel (
                 id         SMALLINT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                 action     VARCHAR(255),
                 ip         VARCHAR(45),
                 url        VARCHAR(255),
                 freq       VARCHAR(50),
                 created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                 updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
                 sid        INT,
                 name       VARCHAR(255)
             )
         )SQL" },

         { "record_files", R"SQL(
             CREATE TABLE record_files (
                 id               BINARY(16) PRIMARY KEY,
                 device           INT UNSIGNED,
                 filename         VARCHAR(255),
                 created_at       DATETIME DEFAULT CURRENT_TIMESTAMP,
                 continuous_count INT,
                 file_path        VARCHAR(1024),
                 name             VARCHAR(255)
             )
         )SQL" },

         { "volume_log", R"SQL(
             CREATE TABLE volume_log (
                 id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                 currentVolume INT,
                 level         INT
             )
         )SQL" },

         // (Optional) เพิ่ม controler ถ้าต้องการให้ตรวจด้วย
         { "controler", R"SQL(
             CREATE TABLE controler (
                 id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
                 sipPort        INT,
                 rtpStartPort   INT,
                 keepAlivePeroid INT,
                 clockrate      INT,
                 sipUser        VARCHAR(64),
                 userID         INT,
                 username       VARCHAR(128),
                 password       VARCHAR(128),
                 userlevel      INT,
                 webpassword    VARCHAR(255),
                 enableRecoreder TINYINT(1) DEFAULT 0
             )
         )SQL" }
     };

     // 2) ถ้าไม่มีตาราง → สร้าง
     for (auto it = tableCreateDDL.begin(); it != tableCreateDDL.end(); ++it) {
         const QString tableName = it.key();
         const QString createSql = it.value();
         if (!tableExists(db, tableName)) {
             QSqlQuery q(db);
             if (!q.exec(createSql)) {
                 qWarning() << "Failed to create table" << tableName << ":" << q.lastError().text();
             } else {
                 qDebug() << "Created missing table:" << tableName;
             }
         } else {
             qDebug() << "Table exists:" << tableName;
         }
     }

     // 3) นิยาม “คอลัมน์ที่ต้องมี” ของแต่ละตาราง (สำหรับเติมคอลัมน์ที่หาย)
     struct ColumnDef { QString name; QString ddl; };
     QMap<QString, QList<ColumnDef>> requiredColumns = {
         { "device_access", {
             { "id",        "id INT AUTO_INCREMENT PRIMARY KEY" }, // ถ้าโครงต่างจะไม่เพิ่มซ้ำ
             { "port_src",  "port_src VARCHAR(30)" },
             { "ip_src",    "ip_src VARCHAR(30)" },
             { "uri",       "uri VARCHAR(30)" },
             { "device",    "device INT UNSIGNED" },
             { "active",    "active TINYINT(1)" },
         } },

         { "device_group", {
             { "id",         "id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY" },
             { "name",       "name VARCHAR(255)" },
             { "lastupdate", "lastupdate DATETIME" },
             { "visible",    "visible TINYINT(1) NOT NULL DEFAULT 1" },
         } },

         { "device_station", {
             { "id",            "id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY" },
             { "sid",           "sid INT UNSIGNED UNIQUE" },
             { "payload_size",  "payload_size INT" },
             { "terminal_type", "terminal_type INT" },
             { "name",          "name VARCHAR(255)" },
             { "ip",            "ip VARCHAR(255)" },
             { "uri",           "uri VARCHAR(255)" },
             { "freq",          "freq DECIMAL(10,0)" },
             { "ambient",       "ambient TINYINT(1) DEFAULT 0" },
             { "group",         "`group` INT UNSIGNED" },
             { "visible",       "visible TINYINT(1) DEFAULT 0" },
             { "last_access",   "last_access DATETIME" },
             { "storage_path",  "storage_path VARCHAR(512)" }, // ★ สำคัญ: คุณใช้คอลัมน์นี้ในโค้ด update
             { "chunk",         "chunk INT DEFAULT 0" },
             { "updated_at",    "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP" },
         } },

         { "devices", {
             { "id",            "id INT AUTO_INCREMENT PRIMARY KEY" },
             { "deviceId",      "deviceId VARCHAR(255)" },
             { "frequency",     "frequency VARCHAR(255)" },
             { "mode",          "mode VARCHAR(50)" },
             { "ip",            "ip VARCHAR(50)" },
             { "timeInterval",  "timeInterval INT" },
             { "pathDirectory", "pathDirectory VARCHAR(255)" },
             { "companyName",   "companyName VARCHAR(255)" },
         } },

         { "record_channel", {
             { "id",         "id SMALLINT NOT NULL AUTO_INCREMENT PRIMARY KEY" },
             { "action",     "action VARCHAR(255)" },
             { "ip",         "ip VARCHAR(45)" },
             { "url",        "url VARCHAR(255)" },
             { "freq",       "freq VARCHAR(50)" },
             { "created_at", "created_at DATETIME DEFAULT CURRENT_TIMESTAMP" },
             { "updated_at", "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP" },
             { "sid",        "sid INT" },
             { "name",       "name VARCHAR(255)" },
         } },

         { "record_files", {
             { "id",               "id BINARY(16) PRIMARY KEY" },
             { "device",           "device INT UNSIGNED" },
             { "filename",         "filename VARCHAR(255)" },
             { "created_at",       "created_at DATETIME DEFAULT CURRENT_TIMESTAMP" },
             { "continuous_count", "continuous_count INT" },
             { "file_path",        "file_path VARCHAR(1024)" }, // ★ เพิ่มตามที่ฐานข้อมูลคุณมีจริง
             { "name",             "name VARCHAR(255)" },       // ★ เพิ่มตามฐาน
         } },

         { "volume_log", {
             { "id",            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY" },
             { "currentVolume", "currentVolume INT" },
             { "level",         "level INT" },
         } },

         // (ถ้าอยากเช็ค controler ด้วย)
         { "controler", {
             { "id",               "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY" },
             { "sipPort",          "sipPort INT" },
             { "rtpStartPort",     "rtpStartPort INT" },
             { "keepAlivePeroid",  "keepAlivePeroid INT" },
             { "clockrate",        "clockrate INT" },
             { "sipUser",          "sipUser VARCHAR(64)" },
             { "userID",           "userID INT" },
             { "username",         "username VARCHAR(128)" },
             { "password",         "password VARCHAR(128)" },
             { "userlevel",        "userlevel INT" },
             { "webpassword",      "webpassword VARCHAR(255)" },
             { "enableRecoreder",  "enableRecoreder TINYINT(1) DEFAULT 0" },
         } },
     };

     // 4) เติมคอลัมน์ที่ขาดในทุกตาราง
     for (auto it = requiredColumns.begin(); it != requiredColumns.end(); ++it) {
         const QString table = it.key();
         if (!tableExists(db, table)) {
             qWarning() << "Skip column check; table does not exist:" << table;
             continue;
         }
         const auto cols = it.value();
         for (const auto& c : cols) {
             // พยายาม ADD COLUMN เฉพาะกรณี “ไม่มีคอลัมน์”
             addColumnIfMissing(db, table, c.name, c.ddl);
         }
     }

     // 5) (ถ้าต้อง) แจ้งเตือนชนิดคอลัมน์ไม่ตรง (ข้ามการแก้ชนิดเพื่อความปลอดภัย)
     //    คุณสามารถเสริมฟังก์ชันตรวจชนิดจาก INFORMATION_SCHEMA.COLUMNS แล้ว qWarning()
     //    เพื่อให้รู้ว่ามีของจริงที่ต่างจากที่คาดไว้

     qDebug() << "CheckAndVerifyDatabases finished";
}


void DatabaseiRec::linkRecordChannelWithDeviceStation() {
    if (!db.isOpen() && !db.open()) {
        qDebug() << "❌ Cannot open database:" << db.lastError().text();
        return;
    }

    QSqlQuery selectQuery(db);
    QSqlQuery updateQuery(db);

    QString selectStr = R"(
        SELECT rc.id, ds.name, ds.id AS ds_id
        FROM record_channel rc
        JOIN device_station ds
          ON rc.ip COLLATE utf8mb4_general_ci = ds.ip COLLATE utf8mb4_general_ci
        WHERE (rc.name IS NULL OR rc.name = '')
           OR (rc.sid IS NULL OR rc.sid = 0)
    )";

    if (!selectQuery.exec(selectStr)) {
        qDebug() << "❌ Failed to select unmatched fields:" << selectQuery.lastError().text();
        return;
    }

    while (selectQuery.next()) {
        int rc_id = selectQuery.value("id").toInt();
        QString ds_name = selectQuery.value("name").toString();
        int ds_id = selectQuery.value("ds_id").toInt();

        QString updateStr = R"(
            UPDATE record_channel
            SET name = :name, sid = :sid
            WHERE id = :id
        )";

        updateQuery.prepare(updateStr);
        updateQuery.bindValue(":name", ds_name);
        updateQuery.bindValue(":sid", ds_id);
        updateQuery.bindValue(":id", rc_id);

        if (!updateQuery.exec()) {
            qDebug() << "❌ Failed to update record_channel.id =" << rc_id << ":" << updateQuery.lastError().text();
        } else {
            qDebug() << "✅ Linked record_channel.id =" << rc_id << " with name =" << ds_name << " and sid =" << ds_id;
        }
    }

//    db.close();
}


void DatabaseiRec::linkRecordFilesWithDeviceStationOnce() {
    if (!db.isOpen() && !db.open()) {
        qDebug() << "Cannot open database:" << db.lastError().text();
        return;
    }

    QSqlQuery checkQuery(db);
    QString checkStr = R"(
        SELECT rf.device
        FROM record_files rf
        LEFT JOIN device_station ds ON rf.device = ds.id
        WHERE rf.file_path IS NULL OR rf.file_path = ''
           OR rf.name IS NULL OR rf.name = ''
        LIMIT 1
    )";

    if (!checkQuery.exec(checkStr)) {
        qDebug() << "Failed to check record_files:" << checkQuery.lastError().text();
        return;
    }

    if (!checkQuery.next()) {
        qDebug() << "All record_files already linked. Nothing to do.";
        return;
    }




    QSqlQuery updateQuery(db);
    QString updateStr = R"(
        UPDATE record_files rf
        JOIN device_station ds ON rf.device = ds.id
        SET rf.file_path = ds.storage_path,
            rf.name = ds.name
        WHERE rf.file_path IS NULL OR rf.file_path = ''
           OR rf.name IS NULL OR rf.name = ''
    )";

    if (!updateQuery.exec(updateStr)) {
        qDebug() << "Failed to update record_files:" << updateQuery.lastError().text();
    } else {
        qDebug() << "Linked missing record_files with device_station successfully.";
    }

//    db.close();
}



void DatabaseiRec::CheckandVerifyTable() {
    qDebug() << "CheckandVerifyTable started";

    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }
    QMap<QString, QMap<QString, QString>> expectedSchemas = {
        {
            "devices", {
                { "id", "INT AUTO_INCREMENT PRIMARY KEY" },
                { "deviceId", "VARCHAR(255)" },
                { "frequency", "VARCHAR(255)" },
                { "mode", "VARCHAR(50)" },
                { "ip", "VARCHAR(50)" },
                { "timeInterval", "INT" },
                { "pathDirectory", "VARCHAR(255)" },
                { "companyName", "VARCHAR(255)" }
            }
        },

        {
            "record_files", {
                { "id", "BINARY(16) PRIMARY KEY" },
                { "device", "INT UNSIGNED" },
                { "filename", "VARCHAR(255)" },
                { "file_path", "VARCHAR(255)" },
                { "created_at", "DATETIME DEFAULT CURRENT_TIMESTAMP" },
                { "continuous_count", "INT" },
                { "name", "VARCHAR(255)" }
            }
        },
        {
            "volume_log", {
                { "id", "INT AUTO_INCREMENT PRIMARY KEY" },
                { "currentVolume", "INT" },
                { "level", "INT" }
            }
        },
        {
            "device_station", {
                { "id", "INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY" },
                { "sid", "INT UNSIGNED UNIQUE" },
                { "payload_size", "INT" },
                { "terminal_type", "INT" },
                { "name", "VARCHAR(255)" },
                { "ip", "VARCHAR(255)" },
                { "uri", "VARCHAR(255)" },
                { "freq", "DECIMAL(10,0)" },
                { "ambient", "TINYINT(1) DEFAULT 0" },
                { "group", "INT UNSIGNED" },
                { "visible", "TINYINT(1) DEFAULT 0" },
                { "last_access", "DATETIME" },
                { "chunk", "INT DEFAULT 0" },
                { "updated_at", "TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP" }
            }

        },
        {
            "record_channel", {
                { "id", "INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY" },
                { "action", "VARCHAR(255)" },
                { "ip", "VARCHAR(45)" },
                { "url", "VARCHAR(255)" },
                { "freq", "VARCHAR(50)" },
                { "created_at", "DATETIME DEFAULT CURRENT_TIMESTAMP" },
                { "updated_at", "DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP" },
                { "sid", "INT" },
                { "name", "VARCHAR(255)" }
            }
        },
        {
            "device_group", {
                { "id", "INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY" },
                { "name", "VARCHAR(255)" },
                { "lastupdate", "DATETIME" },
                { "visible", "TINYINT(1) NOT NULL DEFAULT 1" }
            }
        },
        {
            "device_access", {
                { "id", "INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY" },
                { "port_src", "varchar(30)" },
                { "ip_src", "varchar(30)" },
                { "uri", "varchar(30)" },
                { "device", "int unsigned" },
                { "active", "active" }
            }
        }

    };


    for (auto it = expectedSchemas.begin(); it != expectedSchemas.end(); ++it) {
        verifyTableSchema(it.key(), it.value());
    }

}


void DatabaseiRec::verifyTableSchema(const QString &tableName, const QMap<QString, QString> &expectedColumns) {
    QSqlQuery query;
    query.prepare(QString("DESCRIBE %1").arg(tableName));
    if (!query.exec()) {
        qWarning() << "DESCRIBE failed for table" << tableName << ":" << query.lastError().text();
        return;
    }

    QSet<QString> existingColumns;
    while (query.next()) {
        existingColumns.insert(query.value("Field").toString());
    }

    for (auto it = expectedColumns.begin(); it != expectedColumns.end(); ++it) {
        if (!existingColumns.contains(it.key())) {
            QString alter = QString("ALTER TABLE %1 ADD COLUMN %2 %3").arg(tableName, it.key(), it.value());
            QSqlQuery alterQuery;
            if (!alterQuery.exec(alter)) {
                qWarning() << "Failed to add column" << it.key() << "to" << tableName << ":" << alterQuery.lastError().text();
            } else {
                qDebug() << "Added missing column" << it.key() << "to" << tableName;
            }
        }
    }
}

//void DatabaseiRec::checkFlieAndRemoveDB() {
//    qDebug() << "checkFlieAndRemoveDB";

//    // ใช้ชื่อ connection เฉพาะ thread
//    QString connectionName = QString("preview_connection_%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
//    if (QSqlDatabaseiRec::contains(connectionName)) {
//        QSqlDatabaseiRec::removeDatabase(connectionName);
//    }

//    QSqlDatabase db = QSqlDatabaseiRec::addDatabase("QMYSQL", connectionName);
//    db.setDatabaseName("recorder");
//    db.setUserName("root");
//    db.setPassword("OTL324$");
//    db.setHostName("localhost");

//    if (!db.open()) {
//        qWarning() << "Failed to open DB:" << db.lastError().text();
//        return;
//    }

//    // 1. ตรวจสอบวันที่เก่าสุด
//    QSqlQuery minQuery(db);
//    if (!minQuery.exec("SELECT MIN(created_at) FROM record_files")) {
//        qWarning() << "Failed to get MIN(created_at):" << minQuery.lastError().text();
//        db.close();
//        return;
//    }

//    QDateTime oldestDate;
//    if (minQuery.next()) {
//        oldestDate = minQuery.value(0).toDateTime();
//        qDebug() << "Oldest created_at:" << oldestDate;
//    }

//    if (oldestDate.isNull() || oldestDate.daysTo(QDateTime::currentDateTime()) < 30) {
//        qDebug() << "No cleanup needed. Oldest record is within 30 days.";
//        db.close();
//        return;
//    }

//    // 2. นับจำนวนข้อมูลทั้งหมด
//    QSqlQuery countQuery(db);
//    if (!countQuery.exec("SELECT COUNT(*) FROM record_files")) {
//        qWarning() << "Failed to count records:" << countQuery.lastError().text();
//        db.close();
//        return;
//    }

//    int totalRows = 0;
//    if (countQuery.next()) {
//        totalRows = countQuery.value(0).toInt();
//    }

//    int rowsToDelete = qMax(1, int(totalRows * 0.2));  // อย่างน้อย 1 row
//    qDebug() << "Preparing to delete" << rowsToDelete << "of" << totalRows << "rows";

//    // 3. ดึงข้อมูล 20% แรกที่เก่าที่สุด
//    QString selectSql = QString(R"(
//        SELECT id, device, filename, created_at, continuous_count, file_path, name
//        FROM record_files
//        ORDER BY created_at ASC
//        LIMIT %1
//    )").arg(rowsToDelete);

//    QSqlQuery selectQuery(db);
//    if (!selectQuery.exec(selectSql)) {
//        qWarning() << "Failed to select records to delete:" << selectQuery.lastError().text();
//        db.close();
//        return;
//    }

//    QJsonArray jsonArray;
//    QStringList idList;
//    int row = 0;

//    while (selectQuery.next()) {
//        QByteArray idBytes = selectQuery.value("id").toByteArray();
//        QString idHex = QString::fromUtf8(idBytes.toHex());
//        int device = selectQuery.value("device").toInt();
//        QString filename = selectQuery.value("filename").toString();
//        QString created_at = selectQuery.value("created_at").toDateTime().toString("yyyy-MM-dd HH:mm:ss");
//        int count = selectQuery.value("continuous_count").toInt();
//        QString file_path = selectQuery.value("file_path").toString();
//        QString name = selectQuery.value("name").toString();

//        qDebug() << QString("Row %1").arg(++row)
//                 << "id:" << idHex
//                 << "device:" << device
//                 << "filename:" << filename
//                 << "created_at:" << created_at
//                 << "count:" << count
//                 << "file_path:" << file_path
//                 << "name:" << name;

//        QJsonObject obj;
//        obj["id"] = idHex;
//        obj["device"] = device;
//        obj["filename"] = filename;
//        obj["created_at"] = created_at;
//        obj["file_path"] = file_path;

//        jsonArray.append(obj);
//        idList << QString("0x%1").arg(idHex);
//    }

//    QJsonDocument doc(jsonArray);
//    QString msg = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
//    qDebug() << "storageManager:" << msg;

//    if (storageManager) {
//        DiskThreadArgs* args = new DiskThreadArgs;
//        args->instance = this;
//        args->msgs = msg;

//        pthread_t idThread;
//        int ret = pthread_create(&idThread, nullptr, &DatabaseiRec::ThreadFunc, args);
//        if (ret != 0) {
//            qWarning() << "Failed to create thread for checkDiskAndFormat";
//            delete args;  // cleanup if thread not created
//        } else {
//            pthread_detach(idThread);  // ป้องกัน zombie thread
//        }
//    }

//    // หลังจากส่งให้ storageManager แล้ว:
//    if (!idList.isEmpty()) {
//        QString deleteSql = QString("DELETE FROM record_files WHERE id IN (%1)").arg(idList.join(","));
//        QSqlQuery deleteQuery(db);
//        if (!deleteQuery.exec(deleteSql)) {
//            qWarning() << "Failed to delete records:" << deleteQuery.lastError().text();
//        } else {
//            qDebug() << "Deleted" << idList.size() << "rows from record_files";
//        }
//    }
//    db.close();
//    QSqlDatabaseiRec::removeDatabase(connectionName);

//}

void DatabaseiRec::checkFlieAndRemoveDB() {
    qDebug() << "checkFlieAndRemoveDB";

    // ใช้ชื่อ connection เฉพาะ thread
    QString connectionName = QString("preview_connection_%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    if (QSqlDatabase::contains(connectionName)) {
        QSqlDatabase::removeDatabase(connectionName);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL", connectionName);
    db.setDatabaseName("recorder");
    db.setUserName("root");
    db.setPassword("OTL324$");
    db.setHostName("localhost");

    if (!db.open()) {
        qWarning() << "Failed to open DB:" << db.lastError().text();
        return;
    }

    // 1. ตรวจสอบวันที่เก่าสุด
    QSqlQuery minQuery(db);
    if (!minQuery.exec("SELECT MIN(created_at) FROM record_files")) {
        qWarning() << "Failed to get MIN(created_at):" << minQuery.lastError().text();
        db.close();
        return;
    }

    QDateTime oldestDate;
    if (minQuery.next()) {
        oldestDate = minQuery.value(0).toDateTime();
        qDebug() << "Oldest created_at:" << oldestDate;
    }

    if (oldestDate.isNull() || oldestDate.daysTo(QDateTime::currentDateTime()) < 30) {
        qDebug() << "No cleanup needed. Oldest record is within 30 days.";
        db.close();
        return;
    }

    // 2. นับจำนวนข้อมูลทั้งหมด
    QSqlQuery countQuery(db);
    if (!countQuery.exec("SELECT COUNT(*) FROM record_files")) {
        qWarning() << "Failed to count records:" << countQuery.lastError().text();
        db.close();
        return;
    }

    int totalRows = 0;
    if (countQuery.next()) {
        totalRows = countQuery.value(0).toInt();
    }

    int rowsToDelete = qMax(1, int(totalRows * 0.2));  // อย่างน้อย 1 row
    qDebug() << "Preparing to delete" << rowsToDelete << "of" << totalRows << "rows";

    // 3. ดึงข้อมูล 20% แรกที่เก่าที่สุด
    QString selectSql = QString(R"(
        SELECT id, device, filename, created_at, continuous_count, file_path, name
        FROM record_files
        ORDER BY created_at ASC
        LIMIT %1
    )").arg(rowsToDelete);

    QSqlQuery selectQuery(db);
    if (!selectQuery.exec(selectSql)) {
        qWarning() << "Failed to select records to delete:" << selectQuery.lastError().text();
        db.close();
        return;
    }

    QJsonArray jsonArray;
    QStringList idList;

    // 🔹 เก็บ path จริงของไฟล์ที่จะลบ
    QStringList filePathsToDelete;

    int row = 0;
    while (selectQuery.next()) {
        QByteArray idBytes = selectQuery.value("id").toByteArray();
        QString idHex = QString::fromUtf8(idBytes.toHex());
        int device = selectQuery.value("device").toInt();
        QString filename = selectQuery.value("filename").toString();
        QString created_at = selectQuery.value("created_at").toDateTime().toString("yyyy-MM-dd HH:mm:ss");
        int count = selectQuery.value("continuous_count").toInt();
        QString file_path = selectQuery.value("file_path").toString();   // เช่น "/var/ivoicex"
        QString name = selectQuery.value("name").toString();

        qDebug() << QString("Row %1").arg(++row)
                 << "id:" << idHex
                 << "device:" << device
                 << "filename:" << filename
                 << "created_at:" << created_at
                 << "count:" << count
                 << "file_path:" << file_path
                 << "name:" << name;

        QJsonObject obj;
        obj["id"] = idHex;
        obj["device"] = device;
        obj["filename"] = filename;
        obj["created_at"] = created_at;
        obj["file_path"] = file_path;

        jsonArray.append(obj);
        idList << QString("0x%1").arg(idHex);

        // ========= สร้าง full path ของไฟล์จริงเพื่อเตรียมลบ =========
        // รูปแบบเดียวกับ searchRecordFilesMysql:
        //   file_path + "/" + deviceName + "/" + ymd + "/" + filename
        //   โดย deviceName, ymd parse จาก filename: "24-iGate44-1_20251124_181932_....wav"
        QString fullPath;

        // parse เอา deviceName และ ymd จากชื่อไฟล์
        const QString fnameNoExt = filename.section('.', 0, 0);
        const QStringList parts  = fnameNoExt.split('_');
        QString deviceName, ymd;
        if (parts.size() >= 2) {
            deviceName = parts[0];   // "24-iGate44-1"
            ymd        = parts[1];   // "20251124"
        }

        if (!file_path.isEmpty() && !deviceName.isEmpty() && !ymd.isEmpty()) {
            fullPath = QDir::cleanPath(file_path + "/" + deviceName + "/" + ymd + "/" + filename);
        } else if (!file_path.isEmpty()) {
            // เผื่อกรณี structure ไม่ตรง ให้ลอง simple path
            fullPath = QDir::cleanPath(file_path + "/" + filename);
        }

        if (!fullPath.isEmpty()) {
            filePathsToDelete << fullPath;
            qDebug() << "[checkFlieAndRemoveDB] will try remove file:" << fullPath;
        } else {
            qWarning() << "[checkFlieAndRemoveDB] cannot build full path for" << filename
                       << "file_path =" << file_path;
        }
    }

    QJsonDocument doc(jsonArray);
    QString msg = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    qDebug() << "storageManager:" << msg;

    if (storageManager) {
        DiskThreadArgs* args = new DiskThreadArgs;
        args->instance = this;
        args->msgs = msg;

        pthread_t idThread;
        int ret = pthread_create(&idThread, nullptr, &DatabaseiRec::ThreadFunc, args);
        if (ret != 0) {
            qWarning() << "Failed to create thread for checkDiskAndFormat";
            delete args;  // cleanup if thread not created
        } else {
            pthread_detach(idThread);  // ป้องกัน zombie thread
        }
    }

    // 🔹 4. ลบไฟล์จริงบน disk ก่อน (ใช้ list ที่เราเก็บไว้ด้านบน)
    for (const QString &path : filePathsToDelete) {
        QFile f(path);
        if (f.exists()) {
            if (!f.remove()) {
                qWarning() << "[checkFlieAndRemoveDB] Failed to remove file:" << path;
            } else {
                qDebug() << "[checkFlieAndRemoveDB] Removed file:" << path;
            }
        } else {
            qWarning() << "[checkFlieAndRemoveDB] File not found, skip delete:" << path;
        }
    }

    // 🔹 5. หลังจากจัดการไฟล์บน disk แล้ว ค่อยลบ row ใน DB
    if (!idList.isEmpty()) {
        QString deleteSql = QString("DELETE FROM record_files WHERE id IN (%1)").arg(idList.join(","));
        QSqlQuery deleteQuery(db);
        if (!deleteQuery.exec(deleteSql)) {
            qWarning() << "Failed to delete records:" << deleteQuery.lastError().text();
        } else {
            qDebug() << "Deleted" << idList.size() << "rows from record_files";
        }
    }

    db.close();
    QSqlDatabase::removeDatabase(connectionName);
}


void* DatabaseiRec::ThreadFunc(void* pTr) {
    DiskThreadArgs* args = static_cast<DiskThreadArgs*>(pTr);
    if (args && args->instance && args->instance->storageManager) {
        args->instance->storageManager->checkDiskAndFormat(args->msgs);
    }
    delete args;
    return nullptr;
}


void DatabaseiRec::cleanupOldRecordFiles() {
    if (!db.isOpen() && !db.open()) {
        qWarning() << "Failed to open database for cleanup:" << db.lastError().text();
        return;
    }

    QSqlQuery query;
    QString cleanupSql = R"(
        DELETE FROM record_files
        WHERE created_at < (SELECT MIN(sub.created_at)
                            FROM (
                                SELECT created_at
                                FROM record_files
                                ORDER BY created_at DESC
                                LIMIT 30
                            ) AS sub)
    )";

    if (!query.exec(cleanupSql)) {
        qWarning() << "Cleanup failed:" << query.lastError().text();
    } else {
        qDebug() << "Cleanup completed: old records deleted, only last 30 days kept.";
    }
//    db.close();
}


void DatabaseiRec::maybeRunCleanup() {
    QSqlQuery query;
    if (!query.exec("SELECT MIN(created_at) FROM record_files")) return;

    if (query.next()) {
        QDateTime minDate = query.value(0).toDateTime();
        if (minDate.daysTo(QDateTime::currentDateTime()) >= 30) {
            cleanupOldRecordFiles();
        } else {
            qDebug() << "No cleanup needed. Oldest date is within 30 days.";
        }
    }
}

void DatabaseiRec::CheckAndHandleDevice(const QString& jsonString, QWebSocket* wClient)
{
    qDebug() << "CheckAndHandleDevice:" << jsonString;

    // ---------- เปิด DB ----------
    if (!db.isOpen() && !db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return;
    }

    // ---------- แปลง JSON ----------
    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[CheckAndHandleDevice] JSON parse error:" << perr.errorString();
        return;
    }
    QJsonObject obj = doc.object();

    const QString menuID = obj.value("menuID").toString();
    qDebug() << "[CheckAndHandleDevice] menuID =" << menuID;

    // ===== helper แปลงค่า / เทียบแบบยืดหยุ่น =====
    auto isNullLike = [](const QString &s) -> bool {
        QString t = s.trimmed();
        return t.isEmpty() || t.compare("NULL", Qt::CaseInsensitive) == 0;
    };

    // ถ้า input เป็น "", "NULL", null → ถือว่า "ไม่ใช้ field นี้ตัดสิน changed" (ignore)
    auto equalFlexibleStr = [&](const QString &input, const QString &db) -> bool {
        if (isNullLike(input))
            return true;        // input ว่าง → ไม่ตัดสินว่า mismatch
        if (isNullLike(db) && isNullLike(input))
            return true;
        return input == db;
    };

    // int แบบยืดหยุ่น: ถ้า input == -1 แปลว่า "ไม่สนใจ/ไม่ใช้ field นี้เทียบ"
    auto equalFlexibleInt = [&](int input, int db) -> bool {
        if (input == -1)
            return true;
        return input == db;
    };

    auto jsonToIntFlexible = [](const QJsonValue &v, int def) -> int {
        if (v.isNull() || v.isUndefined())
            return def;

        if (v.isDouble())
            return v.toInt();

        if (v.isString()) {
            QString s = v.toString().trimmed();
            if (s.isEmpty() || s.compare("NULL", Qt::CaseInsensitive) == 0)
                return def;
            bool ok = false;
            int val = s.toInt(&ok);
            return ok ? val : def;
        }

        bool ok = false;
        int val = v.toVariant().toInt(&ok);
        return ok ? val : def;
    };

    auto jsonToStringTrim = [](const QJsonValue &v) -> QString {
        if (v.isNull() || v.isUndefined())
            return QString();
        return v.toString().trimmed();
    };

    // ---------- ดึงค่าจาก JSON ----------
    // sid ใช้แบบปกติ (key)
    int inputSid = jsonToIntFlexible(obj.value("sid"), -1);
    int     inputPayloadSize  = jsonToIntFlexible(obj.value("payload_size"), -1);
    int     inputTerminalType = jsonToIntFlexible(obj.value("terminal_type"), -1);
    QString inputName         = jsonToStringTrim(obj.value("name"));
    QString inputIp           = jsonToStringTrim(obj.value("ip"));
    QString inputUri          = jsonToStringTrim(obj.value("uri"));
    int     inputFreqDummy    = jsonToIntFlexible(obj.value("freq"), -1); // ไม่ใช้จริง แค่ให้ structure เดิม
    Q_UNUSED(inputFreqDummy);
    QString inputFreqStr      = jsonToStringTrim(obj.value("freq"));      // ใช้ string ตรง ๆ
    QString inputAmbient      = jsonToStringTrim(obj.value("ambient"));
    int     inputGroup        = jsonToIntFlexible(obj.value("group"), -1);
    int     inputVisible      = jsonToIntFlexible(obj.value("visible"), -1);

    QString inputFilePath     = jsonToStringTrim(obj.value("file_path"));
    QString inputStoragePath  = jsonToStringTrim(obj.value("storage_path"));

    // ถ้า storage_path ว่าง → fallback ไป file_path
    if (inputStoragePath.isEmpty())
        inputStoragePath = inputFilePath;

    // last_access → finalLastAccess (format dd/MM/yyyy หรือ "NULL")
    QString inputLastAccessRaw = jsonToStringTrim(obj.value("last_access"));
    QDateTime dtLastAccess = QDateTime::fromString(inputLastAccessRaw, Qt::ISODate);
    QString finalLastAccess;
    if (inputLastAccessRaw.isEmpty() ||
        inputLastAccessRaw.compare("NULL", Qt::CaseInsensitive) == 0) {
        finalLastAccess = "NULL";          // เคสว่าง/NULL
    } else if (dtLastAccess.isValid()) {
        finalLastAccess = dtLastAccess.toString("dd/MM/yyyy");
    } else {
        finalLastAccess = inputLastAccessRaw;
    }
    qDebug() << "finalLastAccess =" << finalLastAccess;

    int     inputChunk      = jsonToIntFlexible(obj.value("chunk"), -1);
    QString inputUpdatedAt  = jsonToStringTrim(obj.value("updated_at"));

    // ---------- ดึงข้อมูลจาก DB ----------
    QSqlQuery query;
    query.prepare(R"(
        SELECT id, sid, payload_size, terminal_type, name, ip, uri, freq,
               ambient, `group`, visible, last_access, storage_path, chunk, updated_at
        FROM device_station
        WHERE sid = :sid
    )");
    query.bindValue(":sid", inputSid);

    if (!query.exec()) {
        qWarning() << "Check device_station failed:" << query.lastError().text();
        return;
    }

    bool deviceFound = false;

    while (query.next()) {
        deviceFound = true;

        int     dbSid        = query.value("sid").toInt();
        int     dbPayload    = query.value("payload_size").toInt();
        int     dbTermType   = query.value("terminal_type").toInt();
        QString dbName       = query.value("name").toString();
        QString dbIp         = query.value("ip").toString();
        QString dbUri        = query.value("uri").toString();
        QString dbFreqStr    = query.value("freq").toString();  // string
        QString dbAmbient    = query.value("ambient").isNull()
                                   ? "NULL"
                                   : query.value("ambient").toString();
        int     dbGroup      = query.value("group").toInt();
        int     dbVisible    = query.value("visible").toInt();
        QString dbLastAccess = query.value("last_access").isNull()
                                   ? "NULL"
                                   : query.value("last_access").toString();
        QString dbStoragePath = query.value("storage_path").isNull()
                                    ? QString()
                                    : query.value("storage_path").toString();
        int     dbChunk      = query.value("chunk").toInt();
        QVariant dbUpdatedAtVar = query.value("updated_at");
        QString dbUpdatedAt  = dbUpdatedAtVar.isNull()
                                   ? QString()
                                   : dbUpdatedAtVar.toString();

        bool changed = false;

        if (!equalFlexibleInt(inputPayloadSize,  dbPayload))    changed = true;
        if (!equalFlexibleInt(inputTerminalType, dbTermType))   changed = true;

        if (!equalFlexibleStr(inputName,         dbName))       changed = true;
        if (!equalFlexibleStr(inputIp,           dbIp))         changed = true;
        if (!equalFlexibleStr(inputUri,          dbUri))        changed = true;
        if (!equalFlexibleStr(inputFreqStr,      dbFreqStr))    changed = true; // freq เป็น string
        if (!equalFlexibleStr(inputAmbient,      dbAmbient))    changed = true;
        if (!equalFlexibleInt(inputGroup,        dbGroup))      changed = true;
        if (!equalFlexibleInt(inputVisible,      dbVisible))    changed = true;

        if (!equalFlexibleStr(finalLastAccess,   dbLastAccess)) changed = true;

        if (!equalFlexibleStr(inputFilePath,     dbStoragePath)) changed = true;
        if (!equalFlexibleStr(inputStoragePath,  dbStoragePath)) changed = true;

        if (!equalFlexibleInt(inputChunk,        dbChunk))      changed = true;

        if (!equalFlexibleStr(inputUpdatedAt,    dbUpdatedAt))  changed = true;

        if (changed) {
            qDebug() << "Data mismatch (flex-check). Updating device_station."
                     << "sid =" << dbSid;
            UpdateDeviceInDatabase(jsonString, wClient);
        } else {
            qDebug() << "Device already exists with matching data (flex-check). No update needed."
                     << "sid =" << dbSid;
        }

        break; // sid น่าจะ unique
    }

    if (!deviceFound) {
        qDebug() << "Device not found. Proceeding to register new device.";
        RegisterDeviceToDatabase(jsonString, wClient);
    }
}

void DatabaseiRec::UpdateDeviceInDatabase(const QString& jsonString, QWebSocket* wClient)
{
    Q_UNUSED(wClient);

    qDebug() << "UpdateDeviceInDatabase:" << jsonString;

    if (!db.isOpen() && !db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return;
    }

    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[UpdateDeviceInDatabase] JSON parse error:" << perr.errorString();
        return;
    }
    const QJsonObject obj = doc.object();
    const QString menuID  = obj.value("menuID").toString();
    qDebug() << "[UpdateDeviceInDatabase] menuID =" << menuID;

    auto toIntStrict = [](const QJsonValue &v, int def, const char *fieldName) -> int {
        if (v.isNull() || v.isUndefined()) {
            qDebug() << "[toIntStrict]" << fieldName << "raw=NULL ->" << def;
            return def;
        }
        if (v.isDouble()) {
            int out = v.toInt();
            qDebug() << "[toIntStrict]" << fieldName << "raw(double)=" << v.toDouble() << "->" << out;
            return out;
        }
        if (v.isString()) {
            QString s = v.toString().trimmed();
            if (s.isEmpty() || s.compare("NULL", Qt::CaseInsensitive) == 0) {
                qDebug() << "[toIntStrict]" << fieldName << "raw(string empty/NULL)=" << s << "->" << def;
                return def;
            }
            bool ok = false;
            int n = s.toInt(&ok);
            if (ok) {
                qDebug() << "[toIntStrict]" << fieldName << "raw(string,int)=" << s << "->" << n;
                return n;
            }
            qDebug() << "[toIntStrict]" << fieldName << "raw(string INVALID)=" << s << "->" << def;
            return def;
        }
        if (v.isBool()) {
            int out = v.toBool() ? 1 : 0;
            qDebug() << "[toIntStrict]" << fieldName << "raw(bool)=" << v.toBool() << "->" << out;
            return out;
        }
        bool ok = false;
        int n = v.toVariant().toInt(&ok);
        if (ok) {
            qDebug() << "[toIntStrict]" << fieldName << "raw(variant)=" << v.toVariant() << "->" << n;
            return n;
        }
        qDebug() << "[toIntStrict]" << fieldName << "raw(unknown)=" << v << "->" << def;
        return def;
    };

    auto toNullable = [](const QJsonValue &v) -> QVariant {
        if (v.isUndefined() || v.isNull()) return QVariant();            // SQL NULL
        if (v.isString()) {
            const QString s = v.toString().trimmed();
            if (s.isEmpty() || s.compare("NULL", Qt::CaseInsensitive) == 0)
                return QVariant();                                       // SQL NULL
            return QVariant(s);
        }
        if (v.isDouble()) return QVariant(v.toInt());
        if (v.isBool())   return QVariant(v.toBool());
        return QVariant();
    };

    auto toDateTimeOrNull = [](const QJsonValue &v) -> QVariant {
        if (v.isUndefined() || v.isNull()) return QVariant();
        if (!v.isString()) return QVariant();
        const QString s = v.toString().trimmed();
        if (s.isEmpty() || s.compare("NULL", Qt::CaseInsensitive) == 0) return QVariant();
        QDateTime dt = QDateTime::fromString(s, Qt::ISODate);
        if (!dt.isValid())
            dt = QDateTime::fromString(s, "dd/MM/yyyy");
        if (dt.isValid()) return QVariant(dt);
        return QVariant(s);
    };

    auto jsonToStringTrim = [](const QJsonValue &v) -> QString {
        if (v.isNull() || v.isUndefined())
            return QString();
        return v.toString().trimmed();
    };

    // key ใช้ sid / id
    const int sid   = toIntStrict(obj.value("sid"), -1, "sid");
    const int rowId = toIntStrict(obj.value("id"),  -1, "id");

    const bool useSid = (sid >= 0);
    const bool useId  = (!useSid && rowId > 0);

    if (!useSid && !useId) {
        qWarning() << "UpdateDeviceInDatabase: cannot determine key. sid raw="
                   << obj.value("sid") << " id raw=" << obj.value("id");
        return;
    }

    if (useSid)
        qDebug() << "[UpdateDeviceInDatabase] Using sid as key:" << sid;
    else
        qDebug() << "[UpdateDeviceInDatabase] Using id as key:" << rowId;

    QString whereClause = useSid ? "sid = :sid" : "id = :id";

    QString sql = QStringLiteral(R"(
        UPDATE device_station
           SET payload_size = :payload_size,
               terminal_type = :terminal_type,
               ip = :ip,
               uri = :uri,
               freq = :freq,
               ambient = :ambient,
               `group` = :groupVal,
               visible = :visible,
               last_access = :last_access,
               storage_path = :storage_path,
               name = :name,
               chunk = :chunk,
               updated_at = NOW()
         WHERE %1
    )").arg(whereClause);

    QSqlQuery q(db);
    q.prepare(sql);

    int payloadSize  = toIntStrict(obj.value("payload_size"),  0,  "payload_size");
    int terminalType = toIntStrict(obj.value("terminal_type"), 0,  "terminal_type");
    int groupVal     = toIntStrict(obj.value("group"),         0,  "group");
    int visible      = toIntStrict(obj.value("visible"),       0,  "visible");
    int chunk        = toIntStrict(obj.value("chunk"),         0,  "chunk");

    QString freqStr  = jsonToStringTrim(obj.value("freq"));

    q.bindValue(":payload_size",  payloadSize);
    q.bindValue(":terminal_type", terminalType);
    q.bindValue(":groupVal",      groupVal);
    q.bindValue(":visible",       visible);
    q.bindValue(":chunk",         chunk);

    q.bindValue(":ip",            jsonToStringTrim(obj.value("ip")));
    q.bindValue(":uri",           jsonToStringTrim(obj.value("uri")));
    q.bindValue(":name",          jsonToStringTrim(obj.value("name")));

    q.bindValue(":freq",          freqStr.isEmpty() ? QVariant() : QVariant(freqStr));
    q.bindValue(":ambient",       toNullable(obj.value("ambient")));
    q.bindValue(":last_access",   toDateTimeOrNull(obj.value("last_access")));

    // fix storage_path
    q.bindValue(":storage_path",  "/var/ivoicex");

    if (useSid)
        q.bindValue(":sid", sid);
    else
        q.bindValue(":id", rowId);

    if (!q.exec()) {
        qWarning() << "Update failed:" << q.lastError().text();
        return;
    }

    qDebug() << "Rows affected (driver-specific):" << q.numRowsAffected();

    // SELECT กลับมาสร้าง JSON
    QString selSql = QStringLiteral(R"(
        SELECT id, sid, name, payload_size, terminal_type, ip, uri,
               freq, ambient, `group`, visible, last_access, storage_path,
               chunk, updated_at
          FROM device_station
         WHERE %1
         LIMIT 1
    )").arg(whereClause);

    QSqlQuery sel(db);
    sel.prepare(selSql);

    if (useSid)
        sel.bindValue(":sid", sid);
    else
        sel.bindValue(":id", rowId);

    if (!sel.exec()) {
        qWarning() << "SELECT after update failed:" << sel.lastError().text();
        return;
    }
    if (!sel.next()) {
        qWarning() << "SELECT after update: no row found for key"
                   << (useSid ? QString("sid=%1").arg(sid)
                              : QString("id=%1").arg(rowId));
        return;
    }

    auto nullToString = [](const QVariant &v) -> QJsonValue {
        return v.isNull() ? QJsonValue("NULL") : QJsonValue(v.toString());
    };

    QJsonObject dev;
    dev["id"]            = sel.value("id").toInt();
    dev["sid"]           = sel.value("sid").toInt();
    dev["name"]          = sel.value("name").toString();
    dev["payload_size"]  = sel.value("payload_size").toInt();
    dev["terminal_type"] = sel.value("terminal_type").toInt();
    dev["ip"]            = sel.value("ip").toString();
    dev["uri"]           = sel.value("uri").toString();
    dev["freq"]          = sel.value("freq").toString();   // เป็น string
    dev["ambient"]       = nullToString(sel.value("ambient"));
    dev["group"]         = sel.value("group").toInt();
    dev["visible"]       = sel.value("visible").toInt();
    dev["last_access"]   = nullToString(sel.value("last_access"));

    QString storageOut = sel.value("storage_path").isNull()
                         ? QString("/var/ivoicex")
                         : sel.value("storage_path").toString();
    dev["file_path"]     = storageOut;
    dev["storage_path"]  = storageOut;

    dev["chunk"]         = sel.value("chunk").toInt();
    dev["updated_at"]    = nullToString(sel.value("updated_at"));

    // ส่งกลับทั้งสองฝั่ง
    {
        QJsonObject outScreen;
        outScreen["menuID"] = "updateDevice";
        outScreen["device"] = dev;
        QString msgScreen = QString::fromUtf8(QJsonDocument(outScreen).toJson(QJsonDocument::Compact));
        qDebug() << "commandMysqlToCpp ->" << msgScreen;
        emit commandMysqlToCpp(msgScreen);
    }

    {
        QJsonObject outWeb;
        outWeb["menuID"] = "updateDeviceWeb";
        outWeb["device"] = dev;
        QString msgWeb = QString::fromUtf8(QJsonDocument(outWeb).toJson(QJsonDocument::Compact));
        qDebug() << "commandMysqlToWeb ->" << msgWeb;
        emit commandMysqlToWeb(msgWeb);
    }
}


void DatabaseiRec::selectRecordChannel(QString jsonString, QWebSocket* wClient) {
    qDebug() << "selectRecordChannel:" << jsonString;
    if (!db.isOpen()) {
        if (!db.open()) {
            qWarning() << "Failed to open DB in selectRecordChannel:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);
    if (!query.exec("SELECT * FROM device_station")) {
        qWarning() << "Select all failed:" << query.lastError().text();
        db.close();
        return;
    }
    QJsonArray deviceArray;
    while(query.next()){

        QJsonObject deviceObj;
        deviceObj["id"] = query.value("id").toInt();
        deviceObj["sid"] = query.value("sid").toInt();
        deviceObj["payload_size"] = query.value("payload_size").toInt();
        deviceObj["terminal_type"] = query.value("terminal_type").toInt();
        deviceObj["name"] = query.value("name").toString();
        deviceObj["ip"] = query.value("ip").toString();
        deviceObj["uri"] = query.value("uri").toString();
        deviceObj["freq"] = query.value("freq").toInt();

        deviceObj["ambient"] = query.value("ambient").isNull() ? QJsonValue("NULL") : query.value("ambient").toInt();
        deviceObj["group"] = query.value("group").toInt();
        deviceObj["visible"] = query.value("visible").toInt();
        deviceObj["file_path"] = query.value("storage_path").toString();
        deviceObj["last_access"] = query.value("last_access").isNull() ? QJsonValue("NULL") : query.value("last_access").toString();

        deviceObj["chunk"] = query.value("chunk").isNull() ? QJsonValue("NULL") : query.value("chunk").toInt();
        deviceObj["updated_at"] = query.value("updated_at").isNull() ? QJsonValue("NULL") : query.value("updated_at").toString();

        deviceArray.append(deviceObj);
    }

    QJsonObject responseObj;
    responseObj["menuID"] = "deviceList";
    responseObj["devices"] = deviceArray;
    QJsonDocument doc(responseObj);
    QString resultJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
//    qDebug() << "Sent to WebSocket:" << resultJson;

    if (wClient) {
        wClient->sendTextMessage(resultJson);
        qDebug() << "Sent to WebSocket:" << resultJson;
    }
//    db.close();
}

void DatabaseiRec::recordChannel(QString jsonString, QWebSocket* wClient) {
    qDebug() << "recordChannel:" << jsonString;

    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QRegularExpression regex(R"((\w+), conn: (\d+), ip: ([\d\.]+), uri: (\w+), freq: (\d+))");
    QRegularExpressionMatch match = regex.match(jsonString);
    if (!match.hasMatch()) {
        qWarning() << "Pattern doesn't match message format.";
        db.close();
        return;
    }

    QString record = match.captured(1);
    int conn = match.captured(2).toInt();
    QString ip = match.captured(3);
    QString url = match.captured(4);
    QString freq = match.captured(5);

    int device_id = -1;
    QSqlQuery idQuery(db);
    idQuery.prepare("SELECT id FROM device_station WHERE ip = :ip");
    idQuery.bindValue(":ip", ip);
    if (idQuery.exec() && idQuery.next()) {
        device_id = idQuery.value(0).toInt();
        qDebug() << "Found device_id:" << device_id << "for ip:" << ip;
    } else {
        qWarning() << "Failed to find device_id for ip:" << ip;
        db.close();
        return;
    }

    QSqlQuery checkQuery(db);
    checkQuery.prepare("SELECT COUNT(*) FROM record_channel WHERE ip = :ip");
    checkQuery.bindValue(":ip", ip);
    if (!checkQuery.exec() || !checkQuery.next()) {
        qWarning() << "Failed to check existing IP:" << checkQuery.lastError().text();
        db.close();
        return;
    }

    int count = checkQuery.value(0).toInt();

    if (count > 0) {
        QSqlQuery updateQuery(db);
        updateQuery.prepare(R"(
            UPDATE record_channel
            SET conn = :conn, record = :record, url = :url, freq = :freq, sid = :sid, updated_at = NOW()
            WHERE ip = :ip
        )");
        updateQuery.bindValue(":conn", conn);
        updateQuery.bindValue(":record", record);
        updateQuery.bindValue(":url", url);
        updateQuery.bindValue(":freq", freq);
        updateQuery.bindValue(":sid", device_id);  // ❗️ถ้าใน DB ยังใช้ชื่อ sid ก็ bind ลง sid ไปก่อน        updateQuery.bindValue(":ip", ip);
        if (!updateQuery.exec()) {
            qWarning() << "Update failed:" << updateQuery.lastError().text();
            db.close();
            return;
        } else {
            qDebug() << "Updated record for IP:" << ip;
        }
    } else {
        QSqlQuery insertQuery(db);
        insertQuery.prepare(R"(
            INSERT INTO record_channel (record, conn, ip, url, freq, sid, created_at, updated_at)
            VALUES (:record, :conn, :ip, :url, :freq, :sid, NOW(), NOW())
        )");
        insertQuery.bindValue(":record", record);
        insertQuery.bindValue(":conn", conn);
        insertQuery.bindValue(":ip", ip);
        insertQuery.bindValue(":url", url);
        insertQuery.bindValue(":freq", freq);
        insertQuery.bindValue(":sid", device_id);  // ❗️ถ้ายังไม่ rename column
        if (!insertQuery.exec()) {
            qWarning() << "Insert failed:" << insertQuery.lastError().text();
            db.close();
            return;
        } else {
            qDebug() << "Inserted new record for IP:" << ip;
        }
    }

//    db.close();
    selectRecordChannel(jsonString, wClient);
}


void DatabaseiRec::getRegisterDevicePage(const QString& jsonString, QWebSocket* wClient)
{
    qDebug() << "Fetching all registered device_station..." << jsonString;

    // ======= ตรวจ menuID จาก jsonString ว่าเป็น QML หรือ Web =======
    QString menuID;
    {
        QJsonParseError perr;
        QJsonDocument pdoc = QJsonDocument::fromJson(jsonString.toUtf8(), &perr);
        if (perr.error == QJsonParseError::NoError && pdoc.isObject()) {
            menuID = pdoc.object().value("menuID").toString();
        }
    }
    const bool isWebRequest =
        (menuID == QLatin1String("getRegisterDevicePageWeb"));

    qDebug() << "[getRegisterDevicePage] menuID =" << menuID
             << "isWebRequest=" << isWebRequest;

    // ======= เปิด DB =======
    if (!db.isOpen() && !db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return;
    }

    QSqlQuery query("SELECT * FROM device_station");
    QJsonArray deviceArray;

    while (query.next()) {
        QJsonObject deviceObj;
        deviceObj["id"]            = query.value("id").toInt();
        deviceObj["sid"]           = query.value("sid").toInt();
        deviceObj["payload_size"]  = query.value("payload_size").toInt();
        deviceObj["terminal_type"] = query.value("terminal_type").toInt();
        deviceObj["name"]          = query.value("name").toString();
        deviceObj["ip"]            = query.value("ip").toString();
        deviceObj["uri"]           = query.value("uri").toString();

        // ---- freq: เก็บ/ส่งเป็น string ตรง ๆ ----
        if (query.value("freq").isNull()) {
            deviceObj["freq"] = QJsonValue("NULL");
        } else {
            deviceObj["freq"] = QJsonValue(query.value("freq").toString());
        }

        // ambient: แปลงเป็น "NULL" หรือ string
        {
            const QVariant v = query.value("ambient");
            if (v.isNull()) {
                deviceObj["ambient"] = QJsonValue("NULL");
            } else {
                deviceObj["ambient"] = QJsonValue(v.toString());
            }
        }

        deviceObj["group"]   = query.value("group").toInt();
        deviceObj["visible"] = query.value("visible").toInt();

        // ---- storage_path / file_path: fix เป็น /var/ivoicex ถ้าไม่มี ----
        QString storagePath = query.value("storage_path").toString().trimmed();
        if (storagePath.isEmpty())
            storagePath = "/var/ivoicex";

        deviceObj["file_path"]    = storagePath;
        deviceObj["storage_path"] = storagePath;

        // --------- last_access: แปลงเป็นวันที่ล้วน (dd/MM/yyyy) หรือ "NULL" ---------
        {
            const QVariant v = query.value("last_access");
            if (v.isNull()) {
                deviceObj["last_access"] = QJsonValue("NULL");
            } else {
                QString onlyDate;
                if (v.type() == QVariant::DateTime) {
                    QDateTime dt = v.toDateTime();
                    if (dt.isValid())
                        onlyDate = dt.date().toString("dd/MM/yyyy");
                } else {
                    QString s = v.toString().trimmed();
                    QDateTime dt = QDateTime::fromString(s, Qt::ISODate);
                    if (!dt.isValid())
                        dt = QDateTime::fromString(s, "yyyy-MM-dd HH:mm:ss");
                    if (!dt.isValid())
                        dt = QDateTime::fromString(s, "yyyy-MM-dd");
                    if (dt.isValid())
                        onlyDate = dt.date().toString("dd/MM/yyyy");
                    else
                        onlyDate = s;
                }
                deviceObj["last_access"] = onlyDate;
            }
        }

        // chunk: null → "NULL", มีค่า → เป็น string หรือ int ก็ได้
        {
            const QVariant v = query.value("chunk");
            if (v.isNull()) {
                deviceObj["chunk"] = QJsonValue("NULL");
            } else {
                deviceObj["chunk"] = QJsonValue(v.toString());
            }
        }

        // updated_at: null → "NULL", ไม่ null → string
        deviceObj["updated_at"] = query.value("updated_at").isNull()
                                  ? QJsonValue("NULL")
                                  : QJsonValue(query.value("updated_at").toString());

        deviceArray.append(deviceObj);
    }

    QJsonObject responseObj;
    responseObj["menuID"]  = "deviceList";
    responseObj["devices"] = deviceArray;

    const QJsonDocument doc(responseObj);
    const QString resultJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

    // ======= ตรงนี้คือจุดสำคัญ: แยกส่ง QML / Web =======
    if (isWebRequest) {
        qDebug() << "[getRegisterDevicePage] send to Web (commandMysqlToWeb):"
                 << resultJson;
        emit commandMysqlToWeb(resultJson);
        Q_UNUSED(wClient);   // ตอนนี้ใช้ broadcast ทั้งหมด
    } else {
        qDebug() << "[getRegisterDevicePage] send to QML (commandMysqlToCpp):"
                 << resultJson;
        emit commandMysqlToCpp(resultJson);
    }
}

//void DatabaseiRec::getRegisterDevicePage(const QString& jsonString, QWebSocket* wClient) {
//    qDebug() << "Fetching all registered device_station..." << jsonString;

//    if (!db.isOpen() && !db.open()) {
//        qWarning() << "Failed to open database:" << db.lastError().text();
//        return;
//    }

//    QSqlQuery query("SELECT * FROM device_station");
//    QJsonArray deviceArray;

//    while (query.next()) {
//        QJsonObject deviceObj;
//        deviceObj["id"]            = query.value("id").toInt();
//        deviceObj["sid"]           = query.value("sid").toInt();
//        deviceObj["payload_size"]  = query.value("payload_size").toInt();
//        deviceObj["terminal_type"] = query.value("terminal_type").toInt();
//        deviceObj["name"]          = query.value("name").toString();
//        deviceObj["ip"]            = query.value("ip").toString();
//        deviceObj["uri"]           = query.value("uri").toString();
//        deviceObj["freq"]          = query.value("freq").toInt();

//        deviceObj["ambient"]       = query.value("ambient").isNull()
//                                     ? QJsonValue("NULL") : query.value("ambient").toInt();
//        deviceObj["group"]         = query.value("group").toInt();
//        deviceObj["visible"]       = query.value("visible").toInt();

//        // map file_path -> storage_path (ตามที่คุณใช้ฝั่ง QML/JSON)
//        deviceObj["file_path"]     = query.value("storage_path").toString();

//        // --------- last_access: แปลงเป็นวันที่ล้วน (dd/MM/yyyy) ---------
//        {
//            const QVariant v = query.value("last_access");
//            if (v.isNull()) {
//                deviceObj["last_access"] = QJsonValue("NULL");
//            } else {
//                QString onlyDate;
//                if (v.type() == QVariant::DateTime) {
//                    QDateTime dt = v.toDateTime();
//                    if (dt.isValid())
//                        onlyDate = dt.date().toString("dd/MM/yyyy");
//                } else {
//                    QString s = v.toString().trimmed();
//                    QDateTime dt = QDateTime::fromString(s, Qt::ISODate);
//                    if (!dt.isValid()) dt = QDateTime::fromString(s, "yyyy-MM-dd HH:mm:ss");
//                    if (!dt.isValid()) dt = QDateTime::fromString(s, "yyyy-MM-dd");
//                    if (dt.isValid())
//                        onlyDate = dt.date().toString("dd/MM/yyyy");
//                    else
//                        onlyDate = s;
//                }
//                deviceObj["last_access"] = onlyDate;
//            }
//        }

//        // ---------------------------------------------------------------

//        deviceObj["chunk"]      = query.value("chunk").isNull()
//                                  ? QJsonValue("NULL") : query.value("chunk").toInt();
//        deviceObj["updated_at"] = query.value("updated_at").isNull()
//                                  ? QJsonValue("NULL") : query.value("updated_at").toString();

//        deviceArray.append(deviceObj);
//    }

//    QJsonObject responseObj;
//    responseObj["menuID"]  = "deviceList";
//    responseObj["devices"] = deviceArray;

//    const QJsonDocument doc(responseObj);
//    const QString resultJson = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));

//    qDebug() << "Sent to WebSocket:" << resultJson;
//    commandMysqlToCpp(resultJson);
//}

void DatabaseiRec::RegisterDeviceToDatabase(const QString& jsonString, QWebSocket* wClient)
{
    qDebug() << "RegisterDeviceToDatabase:" << jsonString;

    if (!db.isOpen() && !db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return;
    }

    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[RegisterDeviceToDatabase] JSON parse error:" << perr.errorString();
        return;
    }
    QJsonObject obj = doc.object();

    const QString menuID  = obj.value("menuID").toString();
    const bool    fromWeb = (menuID == "RegisterDeviceWeb");
    qDebug() << "[RegisterDeviceToDatabase] menuID =" << menuID << "fromWeb=" << fromWeb;

    // helper แปลง int ปกติ (ไม่เกี่ยวกับ freq)
    auto toIntStrict = [](const QJsonValue &v, int def, const char *fieldName) -> int {
        if (v.isNull() || v.isUndefined()) {
            qDebug() << "[toIntStrict]" << fieldName << "raw=NULL ->" << def;
            return def;
        }
        if (v.isDouble()) {
            int out = v.toInt();
            qDebug() << "[toIntStrict]" << fieldName << "raw(double)=" << v.toDouble() << "->" << out;
            return out;
        }
        if (v.isString()) {
            QString s = v.toString().trimmed();
            if (s.isEmpty() || s.compare("NULL", Qt::CaseInsensitive) == 0) {
                qDebug() << "[toIntStrict]" << fieldName << "raw(string empty/NULL)=" << s << "->" << def;
                return def;
            }
            bool ok = false;
            int n = s.toInt(&ok);
            if (ok) {
                qDebug() << "[toIntStrict]" << fieldName << "raw(string,int)=" << s << "->" << n;
                return n;
            }
            qDebug() << "[toIntStrict]" << fieldName << "raw(string INVALID)=" << s << "->" << def;
            return def;
        }
        if (v.isBool()) {
            int out = v.toBool() ? 1 : 0;
            qDebug() << "[toIntStrict]" << fieldName << "raw(bool)=" << v.toBool() << "->" << out;
            return out;
        }
        bool ok = false;
        int n = v.toVariant().toInt(&ok);
        if (ok) {
            qDebug() << "[toIntStrict]" << fieldName << "raw(variant)=" << v.toVariant() << "->" << n;
            return n;
        }
        qDebug() << "[toIntStrict]" << fieldName << "raw(unknown)=" << v << "->" << def;
        return def;
    };

    auto toNullable = [](const QJsonValue &v) -> QVariant {
        if (v.isUndefined() || v.isNull()) return QVariant();            // SQL NULL
        if (v.isString()) {
            const QString s = v.toString().trimmed();
            if (s.isEmpty() || s.compare("NULL", Qt::CaseInsensitive) == 0)
                return QVariant();                                       // SQL NULL
            return QVariant(s);
        }
        if (v.isDouble()) return QVariant(v.toInt());                    // เก็บเป็น int
        if (v.isBool())   return QVariant(v.toBool());
        return QVariant();                                               // fallback = NULL
    };

    auto toDateTimeOrNull = [](const QJsonValue &v) -> QVariant {
        if (v.isUndefined() || v.isNull()) return QVariant(); // NULL
        if (!v.isString()) return QVariant();                 // NULL ถ้าไม่ใช่สตริง
        const QString s = v.toString().trimmed();
        if (s.isEmpty() || s.compare("NULL", Qt::CaseInsensitive) == 0) return QVariant();
        QDateTime dt = QDateTime::fromString(s, Qt::ISODate);
        if (!dt.isValid())
            dt = QDateTime::fromString(s, "dd/MM/yyyy");
        if (dt.isValid()) return QVariant(dt);
        return QVariant(s);
    };

    auto jsonToStringTrim = [](const QJsonValue &v) -> QString {
        if (v.isNull() || v.isUndefined())
            return QString();
        return v.toString().trimmed();
    };

    // ---------- อ่านค่าจาก JSON ----------
    int sid           = toIntStrict(obj.value("sid"),           0,  "sid");
    int payloadSize   = toIntStrict(obj.value("payload_size"),  0,  "payload_size");
    int terminalType  = toIntStrict(obj.value("terminal_type"), 0,  "terminal_type");
    QString name      = jsonToStringTrim(obj.value("name"));
    QString ip        = jsonToStringTrim(obj.value("ip"));
    QString uri       = jsonToStringTrim(obj.value("uri"));
    QString freqStr   = jsonToStringTrim(obj.value("freq")); // เก็บเป็น string
    QVariant ambient  = toNullable(obj.value("ambient"));
    int group         = toIntStrict(obj.value("group"),        0,  "group");
    int visible       = toIntStrict(obj.value("visible"),      1,  "visible");
    QVariant lastAcc  = toDateTimeOrNull(obj.value("last_access"));
    int chunk         = toIntStrict(obj.value("chunk"),        -1, "chunk");

    QVariant chunkVar = (chunk < 0) ? QVariant() : QVariant(chunk);

    // storage_path fix path
    QString storagePath = "/var/ivoicex";

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO device_station
            (sid, payload_size, terminal_type, name, ip, uri, freq,
             ambient, `group`, visible, last_access, storage_path, chunk)
        VALUES
            (:sid, :payload_size, :terminal_type, :name, :ip, :uri, :freq,
             :ambient, :groupVal, :visible, :last_access, :storage_path, :chunk)
    )");

    query.bindValue(":sid",           sid);
    query.bindValue(":payload_size",  payloadSize);
    query.bindValue(":terminal_type", terminalType);
    query.bindValue(":name",          name);
    query.bindValue(":ip",            ip);
    query.bindValue(":uri",           uri);
    query.bindValue(":freq",          freqStr.isEmpty() ? QVariant() : QVariant(freqStr));
    query.bindValue(":ambient",       ambient);
    query.bindValue(":groupVal",      group);
    query.bindValue(":visible",       visible);
    query.bindValue(":last_access",   lastAcc);
    query.bindValue(":storage_path",  storagePath);
    query.bindValue(":chunk",         chunkVar);

    if (!query.exec()) {
        qWarning() << "Insert failed:" << query.lastError().text();
    } else {
        qDebug() << "Device station registered successfully!";
    }

    // reload list ทั้งสองฝั่ง (ให้ getRegisterDevicePage ใช้ menuID ตัดสินเอง)
    getRegisterDevicePage(jsonString, wClient);
}

void DatabaseiRec::updatePath(const QString& jsonString, QWebSocket* wClient) {
    qDebug() << "updatePath received:" << jsonString;

    QByteArray br = jsonString.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);
    QJsonObject obj = doc.object();

    if (obj["menuID"].toString() != "changePathDirectory") {
        qWarning() << "Invalid menuID.";
        return;
    }

    QString newPath = obj["ChangePathDirectory"].toString();
    if (newPath.isEmpty()) {
        qWarning() << "Empty path received.";
        return;
    }

    if (!db.isOpen() && !db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return;
    }

    // Update all device_station paths
    QSqlQuery updateQuery(db);
    updateQuery.prepare("UPDATE device_station SET storage_path = :path");
    updateQuery.bindValue(":path", newPath);

    if (!updateQuery.exec()) {
        qWarning() << "Failed to update storage_path:" << updateQuery.lastError().text();
    } else {
        qDebug() << "Updated device_station.storage_path to:" << newPath;

        // Now sync record_files.file_path
        QSqlQuery syncQuery(db);
        QString syncSql = R"(
            UPDATE record_files rf
            JOIN device_station ds ON rf.name = ds.name
            SET rf.file_path = ds.storage_path
            WHERE rf.file_path IS NOT NULL
        )";
        if (!syncQuery.exec(syncSql)) {
            qWarning() << "Failed to sync file_path:" << syncQuery.lastError().text();
        } else {
            qDebug() << "✅ record_files.file_path synced successfully.";
        }

        // Send WebSocket reply
        QJsonObject reply;
        reply["menuID"] = "UpdatePathDirectory";
        reply["status"] = "success";
        reply["newPath"] = newPath;
        wClient->sendTextMessage(QJsonDocument(reply).toJson(QJsonDocument::Compact));
    }

//    db.close();
}

void DatabaseiRec::removeRegisterDevice(const QString& jsonString, QWebSocket* wClient)
{
    qDebug() << "Removing registered device_station..." << jsonString;

    // --- parse JSON ---
    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[removeRegisterDevice] JSON parse error:" << perr.errorString();
        return;
    }
    QJsonObject obj = doc.object();

    // helper: แปลงเป็น int แบบยืดหยุ่น (รองรับทั้ง number, "52", "", "NULL")
    auto toIntFlex = [](const QJsonValue &v, int def) -> int {
        if (v.isNull() || v.isUndefined())
            return def;

        if (v.isDouble())
            return v.toInt();

        if (v.isString()) {
            QString s = v.toString().trimmed();
            if (s.isEmpty() || s.compare("NULL", Qt::CaseInsensitive) == 0)
                return def;
            bool ok = false;
            int n = s.toInt(&ok);
            return ok ? n : def;
        }

        return def;
    };

    // ดึง sid, id แบบยืดหยุ่น → รองรับได้ทั้งเลขและสตริง
    int sid = toIntFlex(obj.value("sid"), -1);
    int id  = toIntFlex(obj.value("id"),  -1);

    bool useSid = (sid >= 0);
    bool useId  = (!useSid && id > 0);

    if (!useSid && !useId) {
        qWarning() << "[removeRegisterDevice] No valid key in JSON. sid raw="
                   << obj.value("sid") << " id raw=" << obj.value("id");
        return;
    }

    if (!db.isOpen() && !db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return;
    }

    QString sql;
    if (useSid) {
        sql = "DELETE FROM device_station WHERE sid = :sid";
    } else {
        sql = "DELETE FROM device_station WHERE id = :id";
    }

    QSqlQuery deleteQuery(db);
    deleteQuery.prepare(sql);

    if (useSid) {
        deleteQuery.bindValue(":sid", sid);
        qDebug() << "[removeRegisterDevice] Deleting by sid =" << sid;
    } else {
        deleteQuery.bindValue(":id", id);
        qDebug() << "[removeRegisterDevice] Deleting by id =" << id;
    }

    if (!deleteQuery.exec()) {
        qWarning() << "Failed to delete device_station:" << deleteQuery.lastError().text();
    } else {
        qDebug() << "Deleted device_station rowsAffected =" << deleteQuery.numRowsAffected();
    }

    // reload หน้า
    getRegisterDevicePage(jsonString, wClient);
}

void DatabaseiRec::fetchAllRecordFiles(QString msgs, QWebSocket* wClient)
{
    const int pageSize = 25;
    qDebug() << "fetchAllRecordFiles:" << msgs;

    QJsonDocument doc = QJsonDocument::fromJson(msgs.toUtf8());
    QJsonObject obj   = doc.object();

//    if (obj.value("menuID").toString() != "getRecordFiles")
//        return;

    // ----- เปิด DB -----
    if (!db.isValid()) {
        qWarning() << "Database connection is invalid!";
        return;
    }
    if (!db.isOpen()) {
        if (!db.open()) {
            qWarning() << "Failed to open DB:" << db.lastError().text();
            return;
        }
    }

    // ----- นับจำนวน row ทั้งหมด -----
    QSqlQuery countQuery(db);
    if (!countQuery.exec("SELECT COUNT(*) FROM record_files")) {
        qWarning() << "Count query failed:" << countQuery.lastError().text();
        return;
    }
    int totalRows = 0;
    if (countQuery.next())
        totalRows = countQuery.value(0).toInt();
    int totalPages = (totalRows + pageSize - 1) / pageSize;

    // ----- ดึง page 1 -----
    static const char *sql =
        "SELECT "
        "  record_files.id, "
        "  record_files.device, "
        "  record_files.filename, "
        "  record_files.created_at, "
        "  record_files.continuous_count, "
        "  device_station.storage_path, "
        "  device_station.name "
        "FROM record_files "
        "JOIN device_station ON record_files.device = device_station.id "
        "ORDER BY record_files.created_at DESC "
        "LIMIT :limit OFFSET :offset";

    QSqlQuery q(db);
    if (!q.prepare(sql)) {
        qWarning() << "Prepare failed:" << q.lastError().text();
        return;
    }
    q.bindValue(":limit",  pageSize);
    q.bindValue(":offset", 0);

    if (!q.exec()) {
        qWarning() << "Query failed:" << q.lastError().text();
        return;
    }

    auto bytesToHuman = [](qulonglong bytes) -> QString {
        static const char *suffixes[] = {"B","KB","MB","GB","TB","PB"};
        int i = 0;
        double cnt = static_cast<double>(bytes);
        while (cnt >= 1024.0 && i < 5) {
            cnt /= 1024.0;
            ++i;
        }
        if (i == 0)
            return QString::number(static_cast<qulonglong>(cnt)) + " " + suffixes[i];
        return QString::number(cnt, 'f', (cnt < 10.0 ? 2 : (cnt < 100.0 ? 1 : 0))) +
               " " + suffixes[i];
    };

    // ===== FAST WAV DURATION (อ่าน header เอง) =====
    auto fastWavDurationSec = [](const QString &path) -> double {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return -1.0;

        // RIFF header 12 bytes
        QByteArray hdr = file.read(12);
        if (hdr.size() < 12)
            return -1.0;

        if (memcmp(hdr.constData(), "RIFF", 4) != 0)
            return -1.0;
        if (memcmp(hdr.constData() + 8, "WAVE", 4) != 0)
            return -1.0;

        auto le16 = [](const unsigned char *p) -> quint16 {
            return quint16(p[0]) | (quint16(p[1]) << 8);
        };
        auto le32 = [](const unsigned char *p) -> quint32 {
            return quint32(p[0])
                 | (quint32(p[1]) << 8)
                 | (quint32(p[2]) << 16)
                 | (quint32(p[3]) << 24);
        };

        bool    haveFmt       = false;
        bool    haveData      = false;
        quint16 audioFormat   = 0;
        quint16 numChannels   = 0;
        quint32 sampleRate    = 0;
        quint16 bitsPerSample = 0;
        quint32 dataSize      = 0;

        while (!file.atEnd()) {
            QByteArray chHdr = file.read(8);
            if (chHdr.size() < 8)
                break;

            const unsigned char *ch = reinterpret_cast<const unsigned char*>(chHdr.constData());
            char id[5] = { (char)ch[0], (char)ch[1], (char)ch[2], (char)ch[3], 0 };
            quint32 chunkSize = le32(ch + 4);

            if (chunkSize > 1000000000u)
                break;

            if (strcmp(id, "fmt ") == 0) {
                quint32 need = qMin(chunkSize, (quint32)32);
                QByteArray fmtData = file.read(need);
                if ((quint32)fmtData.size() < need)
                    break;

                const unsigned char *p = reinterpret_cast<const unsigned char*>(fmtData.constData());
                if (fmtData.size() >= 16) {
                    audioFormat   = le16(p + 0);
                    numChannels   = le16(p + 2);
                    sampleRate    = le32(p + 4);
                    bitsPerSample = le16(p + 14);
                    haveFmt       = true;
                }

                qint64 remain = (qint64)chunkSize - (qint64)fmtData.size();
                if (remain > 0)
                    file.seek(file.pos() + remain);
            }
            else if (strcmp(id, "data") == 0) {
                dataSize = chunkSize;
                haveData = true;
                file.seek(file.pos() + chunkSize); // skip payload
            }
            else {
                file.seek(file.pos() + chunkSize); // skip unknown chunks
            }

            if (haveFmt && haveData)
                break;
        }

        if (!haveFmt || !haveData)
            return -1.0;
        if (sampleRate == 0 || bitsPerSample == 0 || numChannels == 0)
            return -1.0;

        quint32 bytesPerFrame = (bitsPerSample / 8u) * numChannels;
        if (bytesPerFrame == 0)
            return -1.0;

        double totalFrames = double(dataSize) / double(bytesPerFrame);
        double durationSec = totalFrames / double(sampleRate);

        if (durationSec < 0.0)
            return -1.0;

        return durationSec;
    };

    // ===== SLOW FALLBACK: ffprobe =====
    auto ffprobeDurationSec = [](const QString &path) -> double {
        QProcess proc;
        QStringList args;
        args << "-v" << "error"
             << "-show_entries" << "format=duration"
             << "-of" << "default=noprint_wrappers=1:nokey=1"
             << path;

        proc.start("ffprobe", args);
        if (!proc.waitForFinished(2000) ||
            proc.exitStatus() != QProcess::NormalExit ||
            proc.exitCode()  != 0) {
            qWarning() << "[ffprobeDurationSec] ffprobe failed for" << path
                       << "err:" << proc.readAllStandardError();
            return -1.0;
        }

        QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        bool ok = false;
        double d = out.toDouble(&ok);
        if (!ok) {
            qWarning() << "[ffprobeDurationSec] parse failed for" << path << "out=" << out;
            return -1.0;
        }
        return d;
    };

    QJsonArray recordsArray;
    bool firstRow = true;
    while (q.next()) {
        // ==== แถวแรกคือ record ล่าสุดสุด ให้เก็บไว้เลย ====
        if (firstRow) {
            m_lastRecordCreatedAt = q.value("created_at").toString();
            m_lastRecordId        = q.value("id").toString();
            qDebug() << "[fetchAllRecordFiles] latest record:"
                     << "id =" << m_lastRecordId
                     << "created_at =" << m_lastRecordCreatedAt;
            firstRow = false;
        }
        // =================================================
        qDebug() << "Last latest record created_at =" << m_lastRecordCreatedAt;
        qDebug() << "Last latest record id         =" << m_lastRecordId;

        const QString storagePath = q.value("storage_path").toString().trimmed();
        const QString filename    = q.value("filename").toString().trimmed();

        const QString noExt      = filename.section('.', 0, 0);
        const QStringList parts  = noExt.split('_');
        const QString deviceName = (parts.size() >= 1) ? parts[0] : "";
        const QString date       = (parts.size() >= 2) ? parts[1] : "";

        const QString realPath =
            QString("%1/%2/%3/%4").arg(storagePath, deviceName, date, filename);
        qDebug() << "realPath:" << realPath;

        QFileInfo fi(realPath);
        const bool exists = fi.exists() && fi.isFile();

        qulonglong sizeBytes      = 0;
        double     durationSecNum = 0.0;
        QString    humanSize;
        QString    durationStr;

        if (exists) {
            // ----- ขนาดไฟล์ (bytes) -----
            sizeBytes = static_cast<qulonglong>(fi.size());
            humanSize = bytesToHuman(sizeBytes);

            // ----- duration: fast header -> ffprobe, แล้วหาร 2 -----
            if (filename.endsWith(".wav", Qt::CaseInsensitive)) {
                double dur = fastWavDurationSec(realPath);
                if (dur < 0.0)
                    dur = ffprobeDurationSec(realPath);

                if (dur >= 0.0) {
                    dur = dur / 2.0; // ★★ ระบบคุณต้องหาร 2 เหมือนฟังก์ชันอื่น ★★
                    durationSecNum = dur;
                    durationStr    = QString::number(dur, 'f', 3);
                }
            }
        } else {
            qDebug() << "[fetchAllRecordFiles] file not found:" << realPath;
        }

        QJsonObject rec;
        rec["id"]               = q.value("id").toString();
        rec["device"]           = q.value("device").toString();
        rec["filename"]         = filename;
        rec["created_at"]       = q.value("created_at").toString();
        rec["continuous_count"] = q.value("continuous_count").toInt();
        rec["file_path"]        = storagePath;
        rec["full_path"]        = realPath;
        rec["name"]             = q.value("name").toString();
        rec["parsed_date"]      = date;
        rec["file_exists"]      = exists;

        if (exists) {
            rec["size_bytes"]   = static_cast<double>(sizeBytes); // ใช้ sum ใน QML
            rec["size_human"]   = humanSize;                      // text สวย ๆ
            rec["duration_sec"] = durationSecNum;                 // duration (หาร 2 แล้ว)
            rec["sizeBytes"] = durationSecNum;                 // duration (หาร 2 แล้ว)
            rec["duration_str"] = durationStr;                    // string
        }

        recordsArray.append(rec);

        qDebug() << "row:"
                 << "exists="       << exists
                 << "realPath="     << realPath
                 << "sizeBytes="    << sizeBytes
                 << "duration_sec=" << durationSecNum;
    }

    QJsonObject result;
    result["objectName"] = "recordFilesChunk";
    result["records"]    = recordsArray;
    result["page"]       = 1;
    result["isLast"]     = (1 == totalPages);
    result["totalPages"] = totalPages;

    const QString message =
        QJsonDocument(result).toJson(QJsonDocument::Compact);

    emit commandMysqlToCpp(message);
    qDebug() << "Sent page 1 with" << recordsArray.size()
             << "records. Total pages:" << totalPages;
}

void DatabaseiRec::nextPageOfRecorderFiles(QString msgs, QWebSocket* wClient)
{
    qDebug() << "nextPageOfRecorderFiles:" << msgs;
    const int pageSize = 25;

    // ---- Parse JSON ----
    const QJsonDocument doc = QJsonDocument::fromJson(msgs.toUtf8());
    const QJsonObject   obj = doc.object();

    if (!obj.contains("page") || !obj["page"].isDouble()) {
        qWarning() << "Invalid page parameter.";
        return;
    }
    int page = obj["page"].toInt();
    if (page < 1) page = 1;
    const int offset = (page - 1) * pageSize;

    const bool interrupSearch = obj.value("interrupSearch").toBool();
    const QString device = obj.value("device").toString().trimmed();

    auto parseIso = [](const QString& s)->QDateTime {
        return QDateTime::fromString(s, "yyyy-MM-ddTHH:mm:ss");
    };
    auto parseUi = [](const QString& s)->QDateTime {
        return QDateTime::fromString(s, "MM/dd/yyyy, HH:mm:ss");
    };

    QDateTime startDT, endDT;
    if (obj.contains("startISO") && obj.contains("endISO")) {
        startDT = parseIso(obj.value("startISO").toString());
        endDT   = parseIso(obj.value("endISO").toString());
    } else if (obj.contains("startDate") && obj.contains("endDate")) {
        startDT = parseUi(obj.value("startDate").toString());
        endDT   = parseUi(obj.value("endDate").toString());
    }
    if (startDT.isValid() && endDT.isValid() && startDT > endDT)
        std::swap(startDT, endDT);

    // ---- Open DB ----
    if (!db.isValid() || !db.isOpen()) {
        if (!db.open()) {
            qWarning() << "Failed to open DB in nextPageOfRecorderFiles:" << db.lastError().text();
            return;
        }
    }

    // ---- Build SQL (2 โหมด) ----
    QString whereSql = " ";
    if (interrupSearch) {
        QStringList conds;
        if (!device.isEmpty())                     conds << "device = :device";
        if (startDT.isValid() && endDT.isValid())  conds << "created_at BETWEEN :startTS AND :endTS";
        else if (startDT.isValid())                conds << "created_at >= :startTS";
        else if (endDT.isValid())                  conds << "created_at <= :endTS";

        if (!conds.isEmpty())
            whereSql = " WHERE " + conds.join(" AND ") + " ";
    }

    const QString selectSql =
        "SELECT id, device, filename, created_at, continuous_count, file_path, name "
        "FROM record_files" +
        whereSql +
        "ORDER BY created_at DESC "
        "LIMIT :limit OFFSET :offset";

    QSqlQuery query(db);
    if (!query.prepare(selectSql)) {
        qWarning() << "Query prepare failed:" << query.lastError().text();
        qWarning() << "SQL was:" << selectSql;
        return;
    }
    if (interrupSearch) {
        if (!device.isEmpty()) query.bindValue(":device", device);
        if (startDT.isValid()) query.bindValue(":startTS", startDT.toString("yyyy-MM-dd HH:mm:ss"));
        if (endDT.isValid())   query.bindValue(":endTS",   endDT.toString("yyyy-MM-dd HH:mm:ss"));
    }
    query.bindValue(":limit",  pageSize);
    query.bindValue(":offset", offset);

    if (!query.exec()) {
        qWarning() << "Query execution failed:" << query.lastError().text();
        qWarning() << "SQL was:" << selectSql;
        return;
    }

    // ===== FAST WAV DURATION (อ่าน header เอง) =====
    auto fastWavDurationSec = [](const QString &path) -> double {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return -1.0;

        // RIFF header 12 bytes
        QByteArray hdr = file.read(12);
        if (hdr.size() < 12)
            return -1.0;

        if (memcmp(hdr.constData(), "RIFF", 4) != 0)
            return -1.0;
        if (memcmp(hdr.constData() + 8, "WAVE", 4) != 0)
            return -1.0;

        auto le16 = [](const unsigned char *p) -> quint16 {
            return quint16(p[0]) | (quint16(p[1]) << 8);
        };
        auto le32 = [](const unsigned char *p) -> quint32 {
            return quint32(p[0])
                 | (quint32(p[1]) << 8)
                 | (quint32(p[2]) << 16)
                 | (quint32(p[3]) << 24);
        };

        bool    haveFmt       = false;
        bool    haveData      = false;
        quint16 audioFormat   = 0;
        quint16 numChannels   = 0;
        quint32 sampleRate    = 0;
        quint16 bitsPerSample = 0;
        quint32 dataSize      = 0;

        while (!file.atEnd()) {
            QByteArray chHdr = file.read(8);
            if (chHdr.size() < 8)
                break;

            const unsigned char *ch = reinterpret_cast<const unsigned char*>(chHdr.constData());
            char id[5] = { (char)ch[0], (char)ch[1], (char)ch[2], (char)ch[3], 0 };
            quint32 chunkSize = le32(ch + 4);

            if (chunkSize > 1000000000u)
                break;

            if (strcmp(id, "fmt ") == 0) {
                quint32 need = qMin(chunkSize, (quint32)32);
                QByteArray fmtData = file.read(need);
                if ((quint32)fmtData.size() < need)
                    break;

                const unsigned char *p = reinterpret_cast<const unsigned char*>(fmtData.constData());
                if (fmtData.size() >= 16) {
                    audioFormat   = le16(p + 0);
                    numChannels   = le16(p + 2);
                    sampleRate    = le32(p + 4);
                    bitsPerSample = le16(p + 14);
                    haveFmt       = true;
                }

                qint64 remain = (qint64)chunkSize - (qint64)fmtData.size();
                if (remain > 0)
                    file.seek(file.pos() + remain);
            }
            else if (strcmp(id, "data") == 0) {
                dataSize = chunkSize;
                haveData = true;
                file.seek(file.pos() + chunkSize); // skip payload
            }
            else {
                file.seek(file.pos() + chunkSize); // skip other chunks
            }

            if (haveFmt && haveData)
                break;
        }

        if (!haveFmt || !haveData)
            return -1.0;
        if (sampleRate == 0 || bitsPerSample == 0 || numChannels == 0)
            return -1.0;

        quint32 bytesPerFrame = (bitsPerSample / 8u) * numChannels;
        if (bytesPerFrame == 0)
            return -1.0;

        double totalFrames = double(dataSize) / double(bytesPerFrame);
        double durationSec = totalFrames / double(sampleRate);

        if (durationSec < 0.0)
            return -1.0;

        return durationSec;
    };

    // ===== SLOW FALLBACK: ffprobe =====
    auto ffprobeDurationSec = [](const QString &path) -> double {
        QProcess proc;
        QStringList args;
        args << "-v" << "error"
             << "-show_entries" << "format=duration"
             << "-of" << "default=noprint_wrappers=1:nokey=1"
             << path;

        proc.start("ffprobe", args);
        if (!proc.waitForFinished(2000) ||
            proc.exitStatus() != QProcess::NormalExit ||
            proc.exitCode()  != 0) {
            qWarning() << "[ffprobeDurationSec] ffprobe failed for" << path
                       << "err:" << proc.readAllStandardError();
            return -1.0;
        }

        QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        bool ok = false;
        double d = out.toDouble(&ok);
        if (!ok) {
            qWarning() << "[ffprobeDurationSec] parse failed for" << path << "out=" << out;
            return -1.0;
        }
        return d;
    };

    // ---- Build response (เติม size/duration และ full_path ให้ตรง) ----
    QJsonArray recordsArray;
    while (query.next()) {
        const QString fileRoot = query.value("file_path").toString().trimmed();
        const QString filename = query.value("filename").toString().trimmed();

        // แตกชื่อเพื่อดึง <deviceName> และ <YYYYMMDD>
        const QString baseNoExt = filename.section('.', 0, 0);
        const QStringList parts = baseNoExt.split('_');
        QString deviceName, ymd;
        if (parts.size() >= 2) {
            deviceName = parts[0];
            ymd        = parts[1];
        }

        // path แบบลึก / แบบสั้น
        QString fullPathDeep  = QDir::cleanPath(fileRoot + "/" + deviceName + "/" + ymd + "/" + filename);
        QString fullPathShort = QDir::cleanPath(fileRoot + "/" + filename);

        QString fullPath;
        QFileInfo fiDeep(fullPathDeep), fiShort(fullPathShort);
        if (fiDeep.exists() && fiDeep.isFile())        fullPath = fullPathDeep;
        else if (fiShort.exists() && fiShort.isFile()) fullPath = fullPathShort;
        else                                           fullPath = fullPathDeep; // default

        QFileInfo fi(fullPath);

        double sizeBytes  = -1.0;
        double sizeKB     = -1.0;
        double durSec     = -1.0;
        QString sizeStr;
        QString durStr;

        if (fi.exists() && fi.isFile()) {
            sizeBytes = static_cast<double>(fi.size());
            sizeKB    = sizeBytes / 1024.0;
            sizeStr   = QString::number(sizeKB, 'f', 3);

            if (filename.endsWith(".wav", Qt::CaseInsensitive)) {
                // 1) ใช้ fast header parser ก่อน
                durSec = fastWavDurationSec(fullPath);

                // 2) ถ้า header พัง ใช้ ffprobe แทน
                if (durSec < 0.0)
                    durSec = ffprobeDurationSec(fullPath);

                // ★★ หาร 2 เหมือนที่บอก ★★
                if (durSec >= 0.0) {
                    durSec = durSec / 2.0;
                    durStr = QString::number(durSec, 'f', 3);
                }
            }
        }

        QJsonObject rec;
        rec["id"]               = query.value("id").toString();
        rec["device"]           = query.value("device").toString();
        rec["filename"]         = filename;
        rec["created_at"]       = query.value("created_at").toString();
        rec["continuous_count"] = query.value("continuous_count").toInt();
        rec["file_path"]        = fileRoot;
        rec["full_path"]        = fullPath;
        rec["name"]             = query.value("name").toString();

        // เพิ่ม parsed_date ให้เหมือนอีกฟังก์ชัน
        if (!ymd.isEmpty())
            rec["parsed_date"] = ymd;

        if (sizeBytes >= 0.0) {
            rec["size_bytes"] = sizeBytes;
            rec["size"]       = sizeStr;
        } else {
            rec["size_bytes"] = 0.0;
            rec["size"]       = "";
        }

        if (durSec >= 0.0) {
            rec["duration_sec"] = durSec;
            rec["duration_str"] = durStr;
        } else {
            rec["duration_sec"] = 0.0;
            rec["duration_str"] = "";
        }

        qDebug() << "[page]" << fullPath
                 << "exists=" << fi.exists()
                 << "sizeBytes=" << sizeBytes
                 << "durSec="    << durSec;

        recordsArray.append(rec);
    }

    QJsonObject responseObj;
    responseObj["objectName"] = "recordFilesChunk";
    responseObj["records"]    = recordsArray;
    responseObj["page"]       = page;
    responseObj["isLast"]     = (recordsArray.size() < pageSize);

    const QString message = QJsonDocument(responseObj).toJson(QJsonDocument::Compact);
    emit commandMysqlToCpp(message);

    qDebug() << "Sent ChangeNextPageOfRecord page" << page << "with" << recordsArray.size() << "records."
             << " searchMode=" << interrupSearch
             << " device=" << device
             << " start=" << (startDT.isValid()? startDT.toString("yyyy-MM-dd HH:mm:ss") : "null")
             << " end="   << (endDT.isValid()?   endDT.toString("yyyy-MM-dd HH:mm:ss")   : "null");
}


//void DatabaseiRec::nextPageOfRecorderFiles(QString msgs, QWebSocket* wClient)
//{
//    qDebug() << "nextPageOfRecorderFiles:" << msgs;
//    const int pageSize = 25;

//    // ---- Parse JSON ----
//    const QJsonDocument doc = QJsonDocument::fromJson(msgs.toUtf8());
//    const QJsonObject   obj = doc.object();

//    if (!obj.contains("page") || !obj["page"].isDouble()) {
//        qWarning() << "Invalid page parameter.";
//        return;
//    }
//    int page = obj["page"].toInt();
//    if (page < 1) page = 1;
//    const int offset = (page - 1) * pageSize;

//    const bool interrupSearch = obj.value("interrupSearch").toBool();
//    const QString device = obj.value("device").toString().trimmed();

//    auto parseIso = [](const QString& s)->QDateTime {
//        return QDateTime::fromString(s, "yyyy-MM-ddTHH:mm:ss");
//    };
//    auto parseUi = [](const QString& s)->QDateTime {
//        return QDateTime::fromString(s, "MM/dd/yyyy, HH:mm:ss");
//    };

//    QDateTime startDT, endDT;
//    if (obj.contains("startISO") && obj.contains("endISO")) {
//        startDT = parseIso(obj.value("startISO").toString());
//        endDT   = parseIso(obj.value("endISO").toString());
//    } else if (obj.contains("startDate") && obj.contains("endDate")) {
//        startDT = parseUi(obj.value("startDate").toString());
//        endDT   = parseUi(obj.value("endDate").toString());
//    }
//    if (startDT.isValid() && endDT.isValid() && startDT > endDT)
//        std::swap(startDT, endDT);

//    // ---- Open DB ----
//    if (!db.isValid() || !db.isOpen()) {
//        if (!db.open()) {
//            qWarning() << "Failed to open DB in nextPageOfRecorderFiles:" << db.lastError().text();
//            return;
//        }
//    }

//    // ---- Build SQL (2 โหมด) ----
//    QString whereSql = " ";
//    if (interrupSearch) {
//        QStringList conds;
//        if (!device.isEmpty()) conds << "device = :device";
//        if (startDT.isValid() && endDT.isValid())      conds << "created_at BETWEEN :startTS AND :endTS";
//        else if (startDT.isValid())                    conds << "created_at >= :startTS";
//        else if (endDT.isValid())                      conds << "created_at <= :endTS";

//        if (!conds.isEmpty())
//            whereSql = " WHERE " + conds.join(" AND ") + " ";
//    }

//    const QString selectSql =
//        "SELECT id, device, filename, created_at, continuous_count, file_path, name "
//        "FROM record_files"
//        + whereSql +
//        "ORDER BY created_at DESC "
//        "LIMIT :limit OFFSET :offset";

//    QSqlQuery query(db);
//    if (!query.prepare(selectSql)) {
//        qWarning() << "Query prepare failed:" << query.lastError().text();
//        qWarning() << "SQL was:" << selectSql;
//        return;
//    }
//    if (interrupSearch) {
//        if (!device.isEmpty()) query.bindValue(":device", device);
//        if (startDT.isValid()) query.bindValue(":startTS", startDT.toString("yyyy-MM-dd HH:mm:ss"));
//        if (endDT.isValid())   query.bindValue(":endTS",   endDT.toString("yyyy-MM-dd HH:mm:ss"));
//    }
//    query.bindValue(":limit",  pageSize);
//    query.bindValue(":offset", offset);

//    if (!query.exec()) {
//        qWarning() << "Query execution failed:" << query.lastError().text();
//        qWarning() << "SQL was:" << selectSql;
//        return;
//    }

//    // ---------- helper: ใช้ ffprobe อ่าน duration ----------
//    auto ffprobeDurationSec = [](const QString &path) -> double {
//        QProcess proc;
//        QStringList args;
//        args << "-v" << "error"
//             << "-show_entries" << "format=duration"
//             << "-of" << "default=noprint_wrappers=1:nokey=1"
//             << path;

//        proc.start("ffprobe", args);
//        if (!proc.waitForFinished(2000) ||
//            proc.exitStatus() != QProcess::NormalExit ||
//            proc.exitCode()  != 0) {
//            qWarning() << "[ffprobeDurationSec] ffprobe failed for" << path
//                       << "err:" << proc.readAllStandardError();
//            return -1.0;
//        }

//        QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
//        bool ok = false;
//        double d = out.toDouble(&ok);
//        if (!ok) {
//            qWarning() << "[ffprobeDurationSec] parse failed for" << path << "out=" << out;
//            return -1.0;
//        }
//        return d;
//    };

//    // ---- Build response (เติม size/duration และ full_path ให้ตรง) ----
//    QJsonArray recordsArray;
//    while (query.next()) {
//        const QString fileRoot = query.value("file_path").toString().trimmed(); // ปกติคือ "/var/ivoicex"
//        const QString filename = query.value("filename").toString().trimmed();

//        // แตกชื่อเพื่อดึง <deviceName> และ <YYYYMMDD>
//        const QString baseNoExt = filename.section('.', 0, 0);
//        const QStringList parts = baseNoExt.split('_');
//        QString deviceName, ymd;
//        if (parts.size() >= 2) {
//            deviceName = parts[0];   // เช่น "23-iGate23-4"
//            ymd        = parts[1];   // เช่น "20250922"
//        }

//        // ประกอบ full path แบบ “ลึก” (ของจริง)
//        QString fullPathDeep  = QDir::cleanPath(fileRoot + "/" + deviceName + "/" + ymd + "/" + filename);
//        QString fullPathShort = QDir::cleanPath(fileRoot + "/" + filename);

//        // เลือก path ที่มีอยู่จริง
//        QString fullPath;
//        QFileInfo fiDeep(fullPathDeep), fiShort(fullPathShort);
//        if (fiDeep.exists() && fiDeep.isFile())          fullPath = fullPathDeep;
//        else if (fiShort.exists() && fiShort.isFile())   fullPath = fullPathShort;
//        else                                             fullPath = fullPathDeep; // default

//        QFileInfo fi(fullPath);

//        double sizeBytes  = -1.0;
//        double sizeKB     = -1.0;
//        double durSec     = -1.0;
//        QString sizeStr;
//        QString durStr;

//        if (fi.exists() && fi.isFile()) {
//            sizeBytes = static_cast<double>(fi.size());
//            sizeKB    = sizeBytes / 1024.0;
//            sizeStr   = QString::number(sizeKB, 'f', 3);

//            if (filename.endsWith(".wav", Qt::CaseInsensitive)) {
//                durSec = ffprobeDurationSec(fullPath);
//                if (durSec >= 0.0)
//                    durStr = QString::number(durSec, 'f', 3);
//            }
//        }

//        // pack JSON row
//        QJsonObject rec;
//        // ถ้าเดิมคุณเก็บ id เป็น string ตรง ๆ ก็ใช้ toString() ได้เลย
//        // ถ้าอยากใช้แบบเดิม (toHex) ก็เก็บไว้ แต่ส่วนใหญ่ใช้ toString() ก็พอ
//        rec["id"]               = query.value("id").toString();
//        rec["device"]           = query.value("device").toString();
//        rec["filename"]         = filename;
//        rec["created_at"]       = query.value("created_at").toString();
//        rec["continuous_count"] = query.value("continuous_count").toInt();
//        rec["file_path"]        = fileRoot;
//        rec["full_path"]        = fullPath;
//        rec["name"]             = query.value("name").toString();

//        // size/duration ใส่ทั้ง numeric + string ให้ใช้ได้ทุกแบบ
//        if (sizeBytes >= 0.0) {
//            rec["size_bytes"] = sizeBytes;
//            rec["size"]       = sizeStr;
//        } else {
//            rec["size_bytes"] = 0.0;
//            rec["size"]       = "";
//        }

//        if (durSec >= 0.0) {
//            rec["duration_sec"] = durSec;
//            rec["duration_str"] = durStr;
//        } else {
//            rec["duration_sec"] = 0.0;
//            rec["duration_str"] = "";
//        }

//        // debug (ช่วยเช็ค)
//        qDebug() << "[page]" << fullPath
//                 << "exists=" << fi.exists()
//                 << "sizeBytes=" << sizeBytes
//                 << "durSec="    << durSec;

//        recordsArray.append(rec);
//    }

//    QJsonObject responseObj;
//    responseObj["objectName"] = "recordFilesChunk";
//    responseObj["records"]    = recordsArray;
//    responseObj["page"]       = page;
//    responseObj["isLast"]     = (recordsArray.size() < pageSize);

//    const QString message = QJsonDocument(responseObj).toJson(QJsonDocument::Compact);
//    emit commandMysqlToCpp(message);

//    qDebug() << "Sent ChangeNextPageOfRecord page" << page << "with" << recordsArray.size() << "records."
//             << " searchMode=" << interrupSearch
//             << " device=" << device
//             << " start=" << (startDT.isValid()? startDT.toString("yyyy-MM-dd HH:mm:ss") : "null")
//             << " end="   << (endDT.isValid()?   endDT.toString("yyyy-MM-dd HH:mm:ss")   : "null");
//}

void DatabaseiRec::recordVolume(double currentVolume, int level) {
    qDebug() << "recordVolume:" << currentVolume << level;
    db.close();
    if (!db.isValid()) {
        qDebug() << "Creating database connection...";
    }

    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery updateQuery;
    updateQuery.prepare("UPDATE volume_log SET currentVolume = :currentVolume, level = :level WHERE id = 1");
    updateQuery.bindValue(":currentVolume", currentVolume);
    updateQuery.bindValue(":level", level);

    if (!updateQuery.exec()) {
        qWarning() << "Update failed:" << updateQuery.lastError().text();
    } else {
        if (updateQuery.numRowsAffected() > 0) {
            qDebug() << "Device updated successfully!";
        } else {
            qWarning() << "Update executed but no row affected. Check deviceId!";
        }
    }
    db.close();
    updateRecordVolume();
}

void DatabaseiRec::updateRecordVolume() {
    qDebug() << "Opening database...[updateRecordVolume]";

    db.close();
    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query("SELECT currentVolume, level FROM volume_log WHERE id = 1");
    if (!query.exec() || !query.next()) {
        qWarning() << "Select failed:" << query.lastError().text();
        return;
    }

    int currentVolume = query.value(0).toInt();
    int level = query.value(1).toInt();

    QJsonObject mainObject;
    mainObject.insert("menuID", "updateRecordVolume");
    mainObject.insert("currentVolume", currentVolume);
    mainObject.insert("level", level);
    QJsonDocument doc(mainObject);
    QString jsonString = doc.toJson(QJsonDocument::Compact);
    qDebug() << "JSON Output:" << jsonString;
    db.close();
    emit previousRecordVolume(jsonString);

}


void DatabaseiRec::getUserLevel(QWebSocket* wClient){
    db.close();
    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);
    if (!query.exec("SELECT * FROM controler")) {
        qWarning() << "Select failed:" << query.lastError().text();
        return;
    }

    QJsonArray usersArray;

    while (query.next()) {
        QJsonObject userLevel;
        userLevel["idUser"]            = query.value(0).toInt();
        userLevel["sipPort"]           = query.value(1).toInt();
        userLevel["rtpStartPort"]      = query.value(2).toInt();
        userLevel["keepAlivePeroid"]   = query.value(3).toInt();
        userLevel["clockrate"]         = query.value(4).toInt();
        userLevel["sipUser"]           = query.value(5).toString();
        userLevel["userID"]            = query.value(6).toInt();
        userLevel["username"]          = query.value(7).toString();
        userLevel["password"]          = query.value(8).toString();
        userLevel["userlevel"]         = query.value(9).toInt();
        userLevel["webpassword"]       = query.value(10).toString();
        userLevel["enableRecorder"]    = query.value(11).toInt();

        usersArray.append(userLevel);
    }

    QJsonObject mainObject;
    mainObject.insert("objectName", "updateUserLevel");
    mainObject.insert("users", usersArray);

    QJsonDocument doc(mainObject);
    QString jsonString = doc.toJson(QJsonDocument::Compact);
    if(wClient){
        qDebug() << "JSON Output:" << jsonString;
        wClient->sendTextMessage(jsonString);
    }
}

void DatabaseiRec::editUserLevel(QString msg, QWebSocket* wClient)
{
    qDebug() << "editUserLevel:" << msg;

    // แปลง JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << parseError.errorString();
        return;
    }

    QJsonObject obj = doc.object();
    QString usernameNew = obj.value("username").toString();
    QString passwordNewPlain = obj.value("password").toString();

    if (usernameNew.isEmpty()) {
        qWarning() << "Username is empty.";
        return;
    }

    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    // ดึงข้อมูล user เดิม
    QSqlQuery selectQuery(db);
    selectQuery.prepare("SELECT username, password FROM controler WHERE username = :username");
    selectQuery.bindValue(":username", usernameNew);

    if (!selectQuery.exec()) {
        qWarning() << "Select failed:" << selectQuery.lastError().text();
        return;
    }

    if (selectQuery.next()) {
        QString usernameOld = selectQuery.value("username").toString();
        QString passwordOldHash = selectQuery.value("password").toString();

        bool needUpdate = false;
        QString updateQueryStr = "UPDATE controler SET ";
        QStringList updates;
        QString mysqlPassword;

        // Check username change (though likely same if searching by new username)
        if (usernameNew != usernameOld) {
            updates << "username = :newUsername";
            needUpdate = true;
        }

        // ถ้ามี password ส่งมา → hash แล้วเปรียบเทียบ
        if (!passwordNewPlain.isEmpty()) {
            QByteArray sha1_first = QCryptographicHash::hash(
                passwordNewPlain.toUtf8(),
                QCryptographicHash::Sha1
            );

            QByteArray sha1_second = QCryptographicHash::hash(
                sha1_first,
                QCryptographicHash::Sha1
            );

            mysqlPassword = "*" + sha1_second.toHex().toUpper();
            qDebug() << "MySQL password hash:" << mysqlPassword;

            // เช็คว่าต่างจาก password เดิมหรือไม่
            if (mysqlPassword != passwordOldHash) {
                updates << "password = :newPassword";
                needUpdate = true;
            }
        }

        if (needUpdate) {
            updateQueryStr += updates.join(", ");
            updateQueryStr += " WHERE username = :oldUsername";

            QSqlQuery updateQuery(db);
            updateQuery.prepare(updateQueryStr);
            updateQuery.bindValue(":oldUsername", usernameOld);

            if (updates.contains("username = :newUsername")) {
                updateQuery.bindValue(":newUsername", usernameNew);
            }
            if (updates.contains("password = :newPassword")) {
                updateQuery.bindValue(":newPassword", mysqlPassword);
            }

            if (!updateQuery.exec()) {
                qWarning() << "Update failed:" << updateQuery.lastError().text();

                QJsonObject reply;
                reply["menuID"] = "applyEditUserResult";
                reply["status"] = "fail";
                reply["reason"] = updateQuery.lastError().text();
                wClient->sendTextMessage(QJsonDocument(reply).toJson(QJsonDocument::Compact));
                return;
            } else {
                qDebug() << "User updated successfully";

                QJsonObject reply;
                reply["menuID"] = "applyEditUserResult";
                reply["status"] = "success";
                wClient->sendTextMessage(QJsonDocument(reply).toJson(QJsonDocument::Compact));
            }
        } else {
            qDebug() << "No changes needed.";

            QJsonObject reply;
            reply["menuID"] = "applyEditUserResult";
            reply["status"] = "no_change";
            wClient->sendTextMessage(QJsonDocument(reply).toJson(QJsonDocument::Compact));
        }

    } else {
        qWarning() << "User not found.";

        QJsonObject reply;
        reply["menuID"] = "applyEditUserResult";
        reply["status"] = "fail";
        reply["reason"] = "User not found";
        wClient->sendTextMessage(QJsonDocument(reply).toJson(QJsonDocument::Compact));
    }
}

void DatabaseiRec::deleteUserLevel(QString msg, QWebSocket* wClient) {
    qDebug() << "deleteUserLevel:" << msg;

    if (!db.isOpen()) {
        qDebug() << "Opening database...";
        if (!db.open()) {
            qWarning() << "Failed to open database:" << db.lastError().text();
            return;
        }
    }

    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8());
    QJsonObject obj = doc.object();
    QString userName = obj["username"].toString();

    if (userName.isEmpty()) {
        qWarning() << "Missing username in input JSON";
        return;
    }

    QSqlQuery query(db);
    query.prepare("DELETE FROM controler WHERE username = :username");
    query.bindValue(":username", userName);

    if (!query.exec()) {
        qWarning() << "Delete failed:" << query.lastError().text();

        // ส่งกลับ WebSocket ว่าลบไม่สำเร็จ
        QJsonObject reply;
        reply["menuID"] = "deleteUserLevelResult";
        reply["status"] = "fail";
        reply["reason"] = query.lastError().text();
        wClient->sendTextMessage(QJsonDocument(reply).toJson(QJsonDocument::Compact));
        return;
    }

    int rowsAffected = query.numRowsAffected();
    qDebug() << "Rows deleted:" << rowsAffected;

    if (rowsAffected > 0) {
        QJsonObject reply;
        reply["menuID"] = "deleteUserLevelResult";
        reply["status"] = "success";
        wClient->sendTextMessage(QJsonDocument(reply).toJson(QJsonDocument::Compact));
    } else {
        QJsonObject reply;
        reply["menuID"] = "deleteUserLevelResult";
        reply["status"] = "fail";
        reply["reason"] = "No user found with given username";
        wClient->sendTextMessage(QJsonDocument(reply).toJson(QJsonDocument::Compact));
    }

    getUserLevel(wClient);
}


void DatabaseiRec::RemoveFile(const QString& jsonString, QWebSocket* wClient) {
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
    if (!doc.isObject()) {
        qWarning() << "Invalid JSON input ";
        return;
    }

    QJsonObject obj = doc.object();
    QString fileName = obj["fileName"].toString();
    QString filePath = obj["filePath"].toString();

    if (fileName.isEmpty() || filePath.isEmpty()) {
        qWarning() << "Missing fileName or filePath in input JSON";
        return;
    }

    if (!db.isOpen() && !db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return;
    }

    QSqlQuery query;
    query.prepare("DELETE FROM record_files WHERE filename = :filename AND file_path = :file_path");
    query.bindValue(":filename", fileName);
    query.bindValue(":file_path", filePath);

    if (!query.exec()) {
        qWarning() << "Failed to remove record:" << query.lastError().text();
        db.close();
        return;
    }

    if (query.numRowsAffected() == 0) {
        qWarning() << "No matching record found to delete.";
    } else {
        qDebug() << "✅ Successfully removed file:" << fileName << "from path:" << filePath;
    }

    db.close();

    QThread::msleep(100); // optional delay

    // 🔁 Send refreshed list back to client
    QJsonObject mainObject;
    mainObject.insert("menuID", "getRecordFiles");
    mainObject.insert("fileName", fileName); // optional: just use as filter
    QJsonDocument docOut(mainObject);
    QString jsonmainString = docOut.toJson(QJsonDocument::Compact);

    fetchAllRecordFiles(jsonmainString, wClient);  // make sure this method is defined
}


void DatabaseiRec::reloadDatabase()
{
//    system("/etc/init.d/mysql stop");
//    system("/etc/init.d/mysql start");
}

void DatabaseiRec::hashletPersonalize()
{
    QString prog = "/bin/bash";//shell
    QStringList arguments;
    QProcess getAddressProcess;
    QString output;

    QString filename = "/tmp/newhashlet/personalize.sh";
    QString data = QString("#!/bin/bash\n"
                           "su - nano2g -s /bin/bash -c \"hashlet -b /dev/i2c-2 personalize\"\n"
                           "echo $? > /tmp/newhashlet/personalize.txt\n");
    system("mkdir -p /tmp/newhashlet");
    QByteArray dataAyyay(data.toLocal8Bit());
    QFile file(filename);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);
    out << dataAyyay;
    file.close();

    arguments << "-c" << QString("sh /tmp/newhashlet/personalize.sh");
    getAddressProcess.start(prog , arguments);
    getAddressProcess.waitForFinished(1000);
    output = getAddressProcess.readAll();
    arguments.clear();
}

void DatabaseiRec::genHashKey()
{
   QString mac = "", challenge = "", meta = "", password = "", serial = "";
   QStringList macList = getMac();
   if (macList.size() >= 3){
       Q_FOREACH (QString macStr, macList)
       {
           if (macStr.contains("mac")){
               mac = macStr.split(":").at(1);
           }
           else if(macStr.contains("challenge")){
               challenge = macStr.split(":").at(1);
           }
           else if(macStr.contains("meta")){
               meta = macStr.split(":").at(1);
           }
       }
       password = getPassword().replace("\n","");
       serial = getSerial().replace("\n","");
   }

   updateHashTable(mac, challenge, meta, serial, password);
}
bool DatabaseiRec::checkHashletNotData()
{
    QString mac = "", challenge = "", meta = "", password = "", serial = "";
    QString query = QString("SELECT mac, challenge, meta, password, serial  FROM hashlet LIMIT 1");
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return false;
    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qWarning() << "c++: ERROR! "  << qry.lastError();
    }else{
        while (qry.next()) {
            mac         = qry.value(0).toString();
            challenge   = qry.value(1).toString();
            meta        = qry.value(2).toString();
            password    = qry.value(3).toString();
            serial      = qry.value(4).toString();
        }
    }
    db.close();

    return ((mac == "")||(challenge == "")||(meta == "")||(serial == "")||(password == ""));
}

void DatabaseiRec::updateHashTable(QString mac, QString challenge ,QString meta, QString serial, QString password)
{
    if ((mac != "")&(challenge != "")&(meta != "")&(serial != "")&(password != "")){
        QString query = QString("UPDATE hashlet SET mac='%1', challenge='%2', meta='%3', serial='%4', password='%5'")
                .arg(mac).arg(challenge).arg(meta).arg(serial).arg(password);
        if (!db.open()) {
            qWarning() << "c++: ERROR! "  << "database error! database can not open.";
            emit databaseError();
            return ;
        }
        QSqlQuery qry;
        qry.prepare(query);
        if (!qry.exec()){
            qWarning() << "c++: ERROR! "  << qry.lastError();
        }
        db.close();
    }
}

QStringList DatabaseiRec::getMac()
{
    QString prog = "/bin/bash";//shell
    QStringList arguments;
    QProcess getAddressProcess;
    QString output;

    QString filename = "/tmp/newhashlet/getmac.sh";
    QString data = QString("#!/bin/bash\n"
                           "su - nano2g -s /bin/bash -c \"hashlet -b /dev/i2c-2 mac --file /home/nano2g/.hashlet\"\n"
                           "echo $? > /tmp/newhashlet/mac.txt\n");
    system("mkdir -p /tmp/newhashlet");
    QByteArray dataAyyay(data.toLocal8Bit());
    QFile file(filename);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);
    out << dataAyyay;
    file.close();

    arguments << "-c" << QString("sh /tmp/newhashlet/getmac.sh");
    getAddressProcess.start(prog , arguments);
    getAddressProcess.waitForFinished(1000);
    output = getAddressProcess.readAll();
    arguments.clear();
    output = output.replace(" ","");
    return output.split("\n");
}
QString DatabaseiRec::getPassword()
{
    QString prog = "/bin/bash";//shell
    QStringList arguments;
    QProcess getAddressProcess;
    QString output;

    QString filename = "/tmp/newhashlet/getpassword.sh";
    QString data = QString("#!/bin/bash\n"
                           "su - nano2g -s /bin/bash -c \"echo ifz8zean6969** | hashlet -b /dev/i2c-2 hmac\"\n"
                           "echo $? > /tmp/newhashlet/password.txt\n");
    system("mkdir -p /tmp/newhashlet");
    QByteArray dataAyyay(data.toLocal8Bit());
    QFile file(filename);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);
    out << dataAyyay;
    file.close();

    arguments << "-c" << QString("sh /tmp/newhashlet/getpassword.sh");
    getAddressProcess.start(prog , arguments);
    getAddressProcess.waitForFinished(1000);
    output = getAddressProcess.readAll();
    arguments.clear();
    return output;
}
QString DatabaseiRec::getSerial()
{
    QString prog = "/bin/bash";//shell
    QStringList arguments;
    QProcess getAddressProcess;
    QString output;

    QString filename = "/tmp/newhashlet/getserial.sh";
    QString data = QString("#!/bin/bash\n"
                           "su - nano2g -s /bin/bash -c \"hashlet -b /dev/i2c-2 serial-num\"\n"
                           "echo $? > /tmp/newhashlet/password.txt\n");
    system("mkdir -p /tmp/newhashlet");
    QByteArray dataAyyay(data.toLocal8Bit());
    QFile file(filename);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);
    out << dataAyyay;
    file.close();

    arguments << "-c" << QString("sh /tmp/newhashlet/getserial.sh");
    getAddressProcess.start(prog , arguments);
    getAddressProcess.waitForFinished(1000);
    output = getAddressProcess.readAll();
    arguments.clear();
    return output;
}

bool DatabaseiRec::passwordVerify(QString password){
    QString query = QString("SELECT password FROM hashlet LIMIT 1");
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return false;
    }
    QString hashPassword;
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }else{
        while (qry.next()) {
            hashPassword = qry.value(0).toString();
        }
    }
    db.close();
    QString prog = "/bin/bash";//shell
    QStringList arguments;
    QProcess getAddressProcess;
    QString output;

    arguments.clear();
    arguments << "-c" << QString("echo %1 | hashlet hmac").arg(password);
    getAddressProcess.start(prog , arguments);
    getAddressProcess.waitForFinished(3000);
    output = getAddressProcess.readAll();
    if (output == "") {
        qDebug() << "output == \"\"";
        return false;
    }else if(!output.contains(hashPassword)){
        qDebug() << "output != hashPassword";
        return false;
    }

    system("mkdir -p /etc/ed137");
    if (verifyMac()){
        qDebug() << "mac true";


        if (hashPassword != ""){
            QString filename = "/etc/ed137/checkpass.sh";
            QString data = QString("#!/bin/bash\n"
                                   "su - nano2g -s /bin/bash -c \"echo $1 | hashlet offline-hmac -r $2\"\n"
                                   "echo $? > /etc/ed137/checkpass\n");
            system("mkdir -p /etc/ed137");

            QByteArray dataAyyay(data.toLocal8Bit());
            QFile file(filename);
            file.open(QIODevice::WriteOnly | QIODevice::Text);
            QTextStream out(&file);
            out << dataAyyay;
            file.close();
            arguments.clear();
            arguments << "-c" << QString("sh /etc/ed137/checkpass.sh %1 %2").arg(password).arg(hashPassword);
            getAddressProcess.start(prog , arguments);
            getAddressProcess.waitForFinished(-1);
            output = getAddressProcess.readAll();
            qDebug() << output;

            arguments.clear();
            arguments << "-c" << QString("cat /etc/ed137/checkpass");
            getAddressProcess.start(prog , arguments);
            getAddressProcess.waitForFinished(-1);
            output = getAddressProcess.readAll();
            qDebug() << output;
            system("rm -r /etc/ed137");
            if (output.contains("0\n")){
                return true;
            }
            return false;
        }

    }else{
        qDebug() << "mac false";
    }
    system("rm -r /etc/ed137");
    return false;
}

bool DatabaseiRec::verifyMac(){
    QString query = QString("SELECT mac, challenge FROM hashlet LIMIT 1");
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return false;
    }
    QString mac, challenge;
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }else{
        while (qry.next()) {
            mac = qry.value(0).toString();
            challenge = qry.value(1).toString();
        }
    }
    db.close();

    QString prog = "/bin/bash";//shell
    QStringList arguments;
    QProcess getAddressProcess;
    QString output;

    QString filename = "/etc/ed137/checkmac.sh";
    QString data = QString("#!/bin/bash\n"
                           "su - nano2g -s /bin/bash -c \"hashlet offline-verify -c $1 -r $2\"\n"
                           "echo $? > /etc/ed137/checkmac\n");
    system("mkdir -p /etc/ed137");
    QByteArray dataAyyay(data.toLocal8Bit());
    QFile file(filename);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);
    out << dataAyyay;
    file.close();

    arguments << "-c" << QString("sh /etc/ed137/checkmac.sh %1 %2").arg(challenge).arg(mac);
    getAddressProcess.start(prog , arguments);
    getAddressProcess.waitForFinished(1000);
    output = getAddressProcess.readAll();

    arguments.clear();
    arguments << "-c" << QString("cat /etc/ed137/checkmac");
    getAddressProcess.start(prog , arguments);
    getAddressProcess.waitForFinished(1000);
    output = getAddressProcess.readAll();
    arguments.clear();

    if (output.contains("0\n"))
        return true;
    return false;
}

bool DatabaseiRec::database_createConnection()
{
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        //emit databaseError();
        return false;
    }
    db.close();
    qDebug() << "Database connected";
    return true;
}
qint64 DatabaseiRec::getTimeDuration(QString filePath)
{
#ifdef HWMODEL_JSNANO
    QString query = QString("SELECT timestamp FROM fileCATISAudio WHERE path='%1' LIMIT 1").arg(filePath);
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return 0;

    }
    QDateTime timestamp;
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }else{
        while (qry.next()) {
            timestamp = qry.value(0).toDateTime();
        }
    }
    db.close();
    qint64 duration = QDateTime::currentDateTime().toSecsSinceEpoch() - timestamp.toSecsSinceEpoch();
    if (duration <= 0) duration=5;
    return duration;
#else
    return 0;
#endif

}
void DatabaseiRec::getLastEvent()
{
#ifdef HWMODEL_JSNANO
    QString lastEvent;
    QDateTime timestamp;
    int timeDuration;
    int id;
    QString query = QString("SELECT timestamp, event, id, duration_sec FROM fileCATISAudio ORDER BY id DESC LIMIT 1");
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return ;

    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }else{
        while (qry.next()) {
            timestamp = qry.value(0).toDateTime();
            lastEvent = qry.value(1).toString();
            id = qry.value(2).toInt();
            timeDuration = qry.value(3).toInt();
        }
    }
    db.close();

    if ((lastEvent == "Standby") & (timeDuration == 0)){
        qint64 duration = QDateTime::currentDateTime().toSecsSinceEpoch() - timestamp.toSecsSinceEpoch();
        QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        QString query = QString("UPDATE fileCATISAudio SET duration_sec='%1' WHERE id='%2'").arg(duration).arg(id);
        if (!db.open()) {
            qWarning() << "c++: ERROR! "  << "database error! database can not open.";
            emit databaseError();
            return ;
        }
        QSqlQuery qry;
        qry.prepare(query);
        if (!qry.exec()){
            qDebug() << qry.lastError();
        }
        db.close();
    }
#else
    return;
#endif
}
void DatabaseiRec::startProject(QString filePath, QString radioEvent)
{

#ifdef HWMODEL_JSNANO
    QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString query = QString("INSERT INTO fileCATISAudio (path, timestamp, duration_sec, event) "
                            "VALUES ('%1', '%2', '%3', '%4')").arg(filePath).arg(timeStamp).arg(0).arg(radioEvent);
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return ;
    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }
    db.close();
#else
    return;
#endif
}

void DatabaseiRec::insertNewAudioRec(QString filePath, QString radioEvent)
{
#ifdef HWMODEL_JSNANO
    if (radioEvent != "Standby")
    {
        getLastEvent();
    }
    QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString query = QString("INSERT INTO fileCATISAudio (path, timestamp, duration_sec, event) "
                            "VALUES ('%1', '%2', '%3', '%4')").arg(filePath).arg(timeStamp).arg(0).arg(radioEvent);
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return ;
    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }
    db.close();
#else
    return;
#endif
}

void DatabaseiRec::updateAudioRec(QString filePath, float avg_level, float max_level)
{
#ifdef HWMODEL_JSNANO
    qint64 duration = getTimeDuration(filePath);
    QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString query = QString("UPDATE fileCATISAudio SET duration_sec='%1',avg_level=%2, max_level=%3 WHERE path='%4'").arg(duration).arg(avg_level).arg(max_level).arg(filePath);
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return ;
    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }
    db.close();
#else
    return;
#endif
}
void DatabaseiRec::removeAudioFile(int lastMin)
{
#ifdef HWMODEL_JSNANO
    QString filePath = "";
    QString timestamp = QDateTime::currentDateTime().addSecs(-(60*lastMin)).toString("yyyy-MM-dd hh:mm:ss");
    QString query = QString("SELECT path FROM fileCATISAudio WHERE timestamp<'%1' ORDER BY id ASC").arg(timestamp);
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return ;

    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }else{
        while (qry.next()) {
            filePath = qry.value(0).toString();
            if (filePath.contains("/home/pi/")){
                QString commanRm = QString("rm -f %1*").arg(filePath);
                system(commanRm.toStdString().c_str());
            }
        }
    }
    query = QString("DELETE FROM fileCATISAudio WHERE timestamp<'%1'").arg(timestamp);
    qry.prepare(query);
    if (!qry.exec()){
       qDebug() << qry.lastError();
    }else{
       while (qry.next()) {
           filePath = qry.value(0).toString();
           QString commanRm = QString("rm -f %1*").arg(filePath);
           system(commanRm.toStdString().c_str());
       }
    }
    db.close();
#else
    return;
#endif
}

QString DatabaseiRec::getNewFile(int warnPercentFault)
{
#ifdef HWMODEL_JSNANO
    QString filePath = "";
    QString query = QString("SELECT path, id FROM fileCATISAudio WHERE event='PTT On' AND id>%1 AND avg_level>%2 ORDER BY id ASC LIMIT 1").arg(currentFileID).arg(warnPercentFault);
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return "";

    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }else{
        while (qry.next()) {
            filePath = qry.value(0).toString();
            currentFileID = qry.value(1).toInt();
        }
    }
    db.close();
    return filePath;
#else
    return "";
#endif
}

qint64 DatabaseiRec::getStandbyDuration()
{
#ifdef HWMODEL_JSNANO
    qint64 duration_sec = 0;
    QString query = QString("SELECT duration_sec, id FROM fileCATISAudio WHERE event='Standby' AND id>%1  ORDER BY id ASC LIMIT 1").arg(currentFileID);
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return 0;

    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }else{
        while (qry.next()) {
            duration_sec = qry.value(0).toLongLong();
            currentFileID = qry.value(1).toInt();
        }
    }
    db.close();
    return duration_sec;
#else
    return 0;
#endif
}

bool DatabaseiRec::getLastEventCheckAudio(int time, int percentFault, int lastPttMinute)
{
#ifdef HWMODEL_JSNANO
//    qDebug() << "check Last Event And Audio Fault.";
    float avg_level = 0;
    float max_level = 0;
    float last_avg_level = 0;
    float last_max_level = 0;
    QDateTime timestamp = QDateTime::fromSecsSinceEpoch(0);
    QString refDateTime = QDateTime::currentDateTime().addSecs(-(60*lastPttMinute)).toString("yyyy-MM-dd hh:mm:ss");
    float count = 0;
    QString query = QString("SELECT avg_level, max_level, timestamp FROM fileCATISAudio WHERE event='PTT On' AND timestamp>'%2' ORDER BY timestamp DESC LIMIT %1").arg(time).arg(refDateTime);
    if (!db.open()) {
        qWarning() << "c++: ERROR! "  << "database error! database can not open.";
        emit databaseError();
        return false;

    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()){
        qDebug() << qry.lastError();
    }else{
        while (qry.next()) {
            avg_level += qry.value(0).toFloat();
            max_level += qry.value(1).toFloat();
            last_avg_level = qry.value(0).toFloat();
            last_max_level = qry.value(1).toFloat();
            if (qry.value(2).toDateTime() > timestamp)
                timestamp = qry.value(2).toDateTime();
            count += 1;
        }
    }
    db.close();

    avg_level = avg_level/count;
    max_level = max_level/count;

    if ((last_avg_level >= percentFault) & (QDateTime::currentDateTime().addSecs(-(lastPttMinute*60)) > timestamp)) {
        emit audioFault(false);
        return true;
    }

    if (avg_level < percentFault) {
        emit audioFault(true);
        return false;
    }

    if (QDateTime::currentDateTime().addSecs(-(lastPttMinute*60)) > timestamp) {
        emit audioFault(true);
        return false;
    }
    emit audioFault(false);
    return true;
#else
    return false;
#endif
}
