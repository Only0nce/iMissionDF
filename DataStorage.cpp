#include "Database.h"
#include <QRegularExpression>

namespace {

QString safePathPart(QString value, const QString &fallback = QStringLiteral("item"))
{
    value = value.trimmed();
    value.replace(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}._#-]")), QStringLiteral("_"));
    while (value.contains(QStringLiteral(".."))) {
        value.replace(QStringLiteral(".."), QStringLiteral("_"));
    }
    value.remove(QStringLiteral("/"));
    value.remove(QStringLiteral("\\\\"));

    if (value.isEmpty() || value == QStringLiteral(".") || value == QStringLiteral("..")) {
        value = fallback;
    }

    return value.left(180);
}

QString safeEventFileName(QString value, const QString &fallback = QStringLiteral("event.csv"))
{
    value = QFileInfo(value.trimmed()).fileName();
    return safePathPart(value, fallback);
}

QString safeFtpRootPath(QString value, const QString &fallback = QStringLiteral("event_record"))
{
    value = value.trimmed();
    value.replace(QStringLiteral("\\"), QStringLiteral("/"));

    // This value is inserted into the lftp -e script, so remove command
    // separators/control characters while still allowing a normal FTP path.
    value.remove(QRegularExpression(QStringLiteral("[\\r\\n\\t;\"'`$&|<>]")));

    const bool absolute = value.startsWith(QLatin1Char('/'));
    const QStringList rawParts = value.split(QLatin1Char('/'), QString::SkipEmptyParts);

    QStringList safeParts;
    for (const QString &rawPart : rawParts) {
        QString part = safePathPart(rawPart, QString());
        if (part.isEmpty() || part == QStringLiteral(".") || part == QStringLiteral("..")) {
            continue;
        }
        safeParts << part;
    }

    if (safeParts.isEmpty()) {
        safeParts << safePathPart(fallback, QStringLiteral("event_record"));
    }

    QString result = safeParts.join(QLatin1Char('/'));
    if (absolute) {
        result.prepend(QLatin1Char('/'));
    }
    return result.left(240);
}

QString canonicalEventCategory(QString value)
{
    value = value.trimmed();

    if (value.compare(QStringLiteral("Pattern"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Pattern");
    }
    if (value.compare(QStringLiteral("Periodic"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Periodic");
    }
    if (value.compare(QStringLiteral("Relay"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Relay");
    }
    if (value.compare(QStringLiteral("Surge"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Surge");
    }
    if (value.compare(QStringLiteral("Manual"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Manual");
    }

    return QString();
}

QString cleanAbsolutePath(const QString &path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool isInsideBasePath(const QString &path, const QString &basePath)
{
    const QString cleanPath = cleanAbsolutePath(path);
    QString cleanBase = cleanAbsolutePath(basePath);

    while (cleanBase.endsWith(QStringLiteral("/")) && cleanBase.size() > 1) {
        cleanBase.chop(1);
    }

    return cleanPath == cleanBase || cleanPath.startsWith(cleanBase + QStringLiteral("/"));
}

QStringList safeEventWriteRoots()
{
    return {
        QStringLiteral("/mnt/sdcard/event_records"),
        QStringLiteral("/home/pi/event_records")
    };
}

bool isInsideAllowedEventWritePath(const QString &path)
{
    for (const QString &root : safeEventWriteRoots()) {
        if (isInsideBasePath(path, root)) {
            return true;
        }
    }

    return false;
}

bool parseEventDateTimeParts(const QString &eventDateTime,
                             QString &dateFile,
                             QString &timeFile)
{
    if (eventDateTime.size() < 19) {
        return false;
    }

    dateFile = eventDateTime.left(10);
    timeFile = eventDateTime.mid(11, 8);

    if (!QDate::fromString(dateFile, QStringLiteral("yyyy-MM-dd")).isValid() ||
        !QTime::fromString(timeFile, QStringLiteral("HH:mm:ss")).isValid()) {
        return false;
    }

    timeFile.replace(QStringLiteral(":"), QStringLiteral("-"));
    return true;
}

bool ensureDirExists(const QString &path)
{
    return QDir().mkpath(QDir::cleanPath(path));
}

QString safeJoinEventPath(const QString &eventRoot,
                          const QString &category,
                          const QString &dateFile,
                          const QString &timeFile,
                          const QString &fileName = QString())
{
    const QString cleanCategory = canonicalEventCategory(category);
    if (cleanCategory.isEmpty()) {
        return QString();
    }

    QString path = QDir::cleanPath(eventRoot + QStringLiteral("/") + cleanCategory +
                                   QStringLiteral("/") + safePathPart(dateFile, QStringLiteral("date")) +
                                   QStringLiteral("/") + safePathPart(timeFile, QStringLiteral("time")));

    if (!fileName.isEmpty()) {
        path = QDir(path).filePath(safeEventFileName(fileName));
    }

    if (!isInsideAllowedEventWritePath(path)) {
        qWarning() << "[SafePath] blocked event path:" << path;
        return QString();
    }

    return path;
}

} // namespace

bool Database::gettowerAndDistance() {
    //    TowerAndDistance towerData;
    QString query = QString("SELECT substation, direction, linenumber FROM towerAndDistance");

    if (!db.open()) {
        qDebug() << "database error! database can not open.";
        //        emit mysqlError();
        restartMysql();
        return false;
    }
    QSqlQuery qry;
    qry.prepare(query);
    if (!qry.exec()) {
        qDebug() << qry.lastError();
    }

    else {
        while (qry.next()) {
            towerAndDistance->substation = qry.value(0).toString();
            towerAndDistance->direction = qry.value(1).toString();
            towerAndDistance->linenumber = qry.value(2).toString();
            qDebug() << "towerAndDistance" << towerAndDistance->substation << towerAndDistance->direction << towerAndDistance->linenumber;
        }
    }
    db.close();
    return true;
}
////////////////////pattern datastorage//////////////////////////
/// \brief Database::getTimeDuration
void Database::getdatapatternDataDb() {
    //    qDebug()<<"rrrrrrrrrpppopop";
    qDebug() << "Fetching all event records from database...";

    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }
    QSqlQuery query("SELECT e.event_datetime, e.filename, c.name AS category_name FROM event_records e JOIN categories c ON e.category_id = c.id ORDER BY e.event_datetime", db);
    if (!query.exec()) {
        qDebug() << "Failed to execute query:" << query.lastError().text();
        return;
    }
    db.close();
    qDebug() << "Searching for filename:";
    patternDataDb(query);
}

void Database::patternDataDb(QSqlQuery query) {
    qDebug() << "Fetching sorted event records from database...";

    int count = 0;
    while (query.next()) {
        QJsonObject recordObject;
        recordObject["objectName"] = "PatternData";
        recordObject["event_datetime"] = query.value("event_datetime").toString();
        recordObject["filename"] = query.value("filename").toString();
        recordObject["category_name"] = query.value("category_name").toString();
        QJsonDocument jsonDoc(recordObject);
        QString jsonString = jsonDoc.toJson(QJsonDocument::Compact);
        emit cmdmsg(jsonString);
        //        qDebug() << "ijijuhdususuichudih";
        count++;
    }

    if (count == 0) {
        qDebug() << "No data found in database!";
    } else {
        qDebug() << "All records have been sent!";
    }
}

void Database::sortByName(bool ascending, const QString &categoryName) {
    qDebug() << "sortByName" << categoryName;
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);
    QString queryStr = QString("SELECT e.event_datetime, e.filename, c.name AS category_name FROM event_records e JOIN categories c ON e.category_id = c.id WHERE c.name = '%1' ORDER BY e.filename %2").arg(categoryName).arg(ascending ? "ASC" : "DESC");

    if (!query.exec(queryStr)) {
        qDebug() << "Query failed:" << query.lastError().text();
        return;
    }

    qDebug() << "Sorting by filename" << (ascending ? " ASC" : " DESC");
    db.close();
    patternDataDb(query);
}
void Database::sortByDate(bool ascending, const QString &categoryName) {
    qDebug() << "sortByName" << categoryName;
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);
    QString queryStr = QString("SELECT e.event_datetime, e.filename, c.name AS category_name FROM event_records e JOIN categories c ON e.category_id = c.id WHERE c.name = '%1'   ORDER BY e.event_datetime %2").arg(categoryName).arg(ascending ? "ASC" : "DESC");

    if (!query.exec(queryStr)) {
        qDebug() << "Query failed:" << query.lastError().text();
        return;
    }

    qDebug() << "Sorting by filename" << (ascending ? " ASC" : " DESC");
    db.close();
    patternDataDb(query);
}
void Database::searchByName(const QString &name, const QString &categoryName) {
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);
    QString queryStr = QString("SELECT e.event_datetime, e.filename, c.name AS category_name  FROM event_records e  JOIN categories c ON e.category_id = c.id  WHERE c.name = '%1'  AND e.filename LIKE '%%2%'   ORDER BY e.event_datetime ASC").arg(categoryName).arg(name);

    if (!query.exec(queryStr)) {
        qDebug() << "Query failed:" << query.lastError().text();
        return;
    }

    qDebug() << "Searching for filename:" << name;
    db.close();
    patternDataDb(query);
}
void Database::searchByDate(const QString &date, const QString &categoryDate) {
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);
    QString queryStr = QString("SELECT e.event_datetime, e.filename, c.name AS category_name  FROM event_records e  JOIN categories c ON e.category_id = c.id  WHERE c.name = '%1'  AND e.event_datetime LIKE '%%2%'   ORDER BY e.event_datetime ASC").arg(categoryDate).arg(date);

    if (!query.exec(queryStr)) {
        qDebug() << "Query failed:" << query.lastError().text();
        return;
    }

    qDebug() << "Searching for filename:" << date;
    db.close();
    patternDataDb(query);
}

void Database::scanFiles() {
    if (!db.isOpen()) return;

    QString basePath = "/home/pi/event_records/";
    QStringList categories = {"Patern", "Periodic", "Relay", "Surge", "Manual"};

    QSqlQuery query(db);

    for (const QString &category : categories) {
        int categoryId = getCategoryId(category);
        QDir categoryDir(basePath + category);
        if (!categoryDir.exists()) continue;

        QStringList dateFolders = categoryDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &dateFolder : dateFolders) {
            QDir dateDir(categoryDir.filePath(dateFolder));
            QStringList files = dateDir.entryList(QStringList() << "*.csv", QDir::Files);

            for (const QString &file : files) {
                QString fullPath = dateDir.filePath(file);

                query.prepare("SELECT COUNT(*) FROM event_records WHERE full_path = ?");
                query.addBindValue(fullPath);
                query.exec();
                query.next();

                if (query.value(0).toInt() == 0) {
                    QFileInfo fileInfo(fullPath);
                    QDateTime fileDateTime = fileInfo.lastModified();

                    query.prepare(
                        "INSERT INTO event_records (category_id, event_datetime, filename, full_path) "
                        "VALUES (?, ?, ?, ?)");
                    query.addBindValue(categoryId);
                    query.addBindValue(fileDateTime.toString("yyyy-MM-dd HH:mm:ss"));
                    query.addBindValue(file);
                    query.addBindValue(fullPath);
                    query.exec();
                }
            }
        }
    }
    qDebug() << "CSV file sync completed.";
}

int Database::getCategoryId(const QString &category) {
    QSqlQuery query(db);
    query.prepare("SELECT id FROM categories WHERE name = ?");
    query.addBindValue(category);
    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    query.prepare("INSERT INTO categories (name) VALUES (?)");
    query.addBindValue(category);
    query.exec();
    return query.lastInsertId().toInt();
}

void Database::renameFileAndUpdateDb(const QString &oldFileName, const QString &newFileName, const QString &category, const QDate &date) {
    QString folderPart = QString("/home/pi/event_record/%1/%2").arg(category).arg(date.toString("dd-MM-yyyy"));
    QDir dir(folderPart);
    QString oldFullPath = dir.filePath(oldFileName);
    QString newFullPath = dir.filePath(newFileName);

    if (!QFile::exists(oldFullPath)) {
        qDebug() << "Old file does not exist:" << oldFullPath;
        return;
    }

    if (QFile::rename(oldFullPath, newFullPath)) {
        qDebug() << "File rename from" << oldFullPath << "to" << newFullPath;
    } else {
        qDebug() << "Faile to rename file from" << oldFullPath << "to" << newFullPath;
    }

    QSqlQuery query;
    query.prepare("UPDATE event_records SET filename = ?, full_path = ? WHERE full_path = ?");
    query.addBindValue(newFileName);
    query.addBindValue(newFullPath);
    query.addBindValue(oldFullPath);

    if (!query.exec()) {
        qDebug() << "Failed to update database:" << query.lastError();
    } else {
        qDebug() << "Database updated successfully!";
    }
}

int Database::getfileNamePattern() {
    QSqlQuery query(db);
    query.prepare("SELECT COUNT(*) + 1 FROM event_records WHERE category_id = 1");

    if (!query.exec()) {
        qDebug() << "Database query failed:" << query.lastError().text();
        return 1;
    }
    if (query.next()) {
        return query.value(0).toInt();
    }

    return 1;
}

void Database::SumNormalizationandUpdateDb(QString modeName, QString phase, QString FileTimeStamp, QJsonArray dist, QJsonArray volt) {
    modeName = canonicalEventCategory(modeName);
    if (modeName.isEmpty()) {
        qWarning() << "[SumNormalizationandUpdateDb] invalid modeName";
        return;
    }

    QString truncatedTimestamp = FileTimeStamp;
    QString TimestampuseinFile = FileTimeStamp;
    TimestampuseinFile.replace(" ", "_");
    TimestampuseinFile.replace(".", "-");
    int dotIndex = truncatedTimestamp.indexOf('.');
    if (dotIndex != -1) {
        truncatedTimestamp = truncatedTimestamp.left(dotIndex + 4);  // Keep 3 digits after the dot
    }

    // หาตำแหน่งเครื่องหมาย -
    int dashPos = TimestampuseinFile.indexOf('-');
    if (dashPos != -1) {
        TimestampuseinFile = TimestampuseinFile.left(dashPos);  // เก็บเฉพาะส่วนก่อนหน้า '-'
    }

    QDateTime dateTime = QDateTime::fromString(truncatedTimestamp, "yyyyMMdd HHmmss.zzz");

    if (dateTime.isValid()) {
        DateKept = dateStr = dateTime.toString("yyyy-MM-dd");
        TimeKept = dateTime.toString("HH-mm-ss");
        timeStr = dateTime.toString("HH:mm:ss.zzz");
    } else {
        qDebug() << "Invalid timestamp format!";
    }
    // qDebug() << "SumNormalizationandUpdateDbDEBUG" << modeName << phase << FileTimeStamp << dist << volt;
    //    qDebug() << "phase" << phase << "modeName" << modeName << "FileTimeStamp" << FileTimeStamp << "dateStr" << dateStr << "timeStr" << timeStr << "TimestampuseinFile" << TimestampuseinFile << "dist" << dist << "volt" << volt;

    //    QString timeStrfile = timeStr;
    QString timeWithoutMs = timeStr.section('.', 0, 0);
    timeWithoutMs.replace(":", "-");
    //    timeStrfile.replace(".", "-");
    QString saveTimeInMysql = QString("%1-%2").arg(dateStr).arg(timeStr);
    QString folderPath = QString(EVENT_PATH + "%1/%2/%3").arg(modeName).arg(dateStr).arg(timeWithoutMs);
    // qWarning() << "Surge folderPath:" << folderPath;
    QDir dir(folderPath);

    if (modeName != "Pattern") {
        if (!dir.exists()) {
            dir.mkpath(".");
            if (QFile::exists(folderPath)) {
                qDebug() << "Directory Created" << endl;
            }
            qDebug() << "create Surge folderPath";
        }
    }

    // qDebug() << "SumNormalizationandUpdateDb::towerAndDistance" << towerAndDistance->substation << towerAndDistance->direction << towerAndDistance->linenumber;

    distMap[phase] = dist;
    voltMap[phase] = volt;


    // qWarning() << "modeName::" << modeName;
    // qWarning() << "gmodeName SetupEquipment->SubstationName" << SetupEquipment->SubstationName
    //            << " SetupEquipment->Voltage" << SetupEquipment->Voltage
    //            << " SetupEquipment->TransmissionLineName" << SetupEquipment->TransmissionLineName
    //            << " SetupEquipment->Distance" << SetupEquipment->Distance
    //            << " SetupEquipment->IPaddress" << SetupEquipment->IPaddress
    //            << " SetupEquipment->Brand" << SetupEquipment->Brand
    //            << " SetupEquipment->Model" << SetupEquipment->Model
    //            << " SetupEquipment->SerialNo" << SetupEquipment->SerialNo
    //            << " SetupEquipment->ContractNumber" << SetupEquipment->ContractNumber
    //            << " SetupEquipment->Date" << SetupEquipment->Date
    //            << " SetupEquipment->LFLSerialNo" << SetupEquipment->LFLSerialNo;
    bool eventCsvReady = false;
    fileNames = "";
    // if(distMap.contains("phaseA") && distMap.contains("phaseB") && distMap.contains("phaseC") && voltMap.contains("phaseA") && voltMap.contains("phaseB") && voltMap.contains("phaseC")){
    if(modeName != "Surge"){
        if (modeName == "Surge") {
            category_id = 4;
            fileNames = QString("%1_%2kV-%3#%4_S%5").arg(SetupEquipment->SubstationName).arg(SetupEquipment->Voltage).arg(SetupEquipment->TransmissionLineName).arg(SetupEquipment->LFLSerialNo).arg(TimestampuseinFile);
            // qWarning() << "modeName Surge::" << modeName << " fileName::" << fileNames
            //            << " SetupEquipment->SubstationName" << SetupEquipment->SubstationName << " SetupEquipment->Voltage" << SetupEquipment->Voltage;
        } else if (modeName == "Relay") {
            category_id = 3;
            fileNames = QString("%1_%2kV-%3#%4_R%5").arg(SetupEquipment->SubstationName).arg(SetupEquipment->Voltage).arg(SetupEquipment->TransmissionLineName).arg(SetupEquipment->LFLSerialNo).arg(TimestampuseinFile);
            // qWarning() << "modeName Surge::" << modeName << " fileName::" << fileNames
            //            << " SetupEquipment->SubstationName" << SetupEquipment->SubstationName << " SetupEquipment->Voltage" << SetupEquipment->Voltage;
        } else if (modeName == "Manual") {
            category_id = 5;
            fileNames = QString("%1_%2kV-%3#%4_M%5").arg(SetupEquipment->SubstationName).arg(SetupEquipment->Voltage).arg(SetupEquipment->TransmissionLineName).arg(SetupEquipment->LFLSerialNo).arg(TimestampuseinFile);
            // qWarning() << "modeName Surge::" << modeName << " fileName::" << fileNames
            //            << " SetupEquipment->SubstationName" << SetupEquipment->SubstationName << " SetupEquipment->Voltage" << SetupEquipment->Voltage;
        } else if (modeName == "Periodic") {
            category_id = 2;
            fileNames = QString("%1_%2kV-%3#%4_P%5").arg(SetupEquipment->SubstationName).arg(SetupEquipment->Voltage).arg(SetupEquipment->TransmissionLineName).arg(SetupEquipment->LFLSerialNo).arg(TimestampuseinFile);
            // qWarning() << "modeName Surge::" << modeName << " fileName::" << fileNames
            //            << " SetupEquipment->SubstationName" << SetupEquipment->SubstationName << " SetupEquipment->Voltage" << SetupEquipment->Voltage;
        } else if (modeName == "Pattern") {
            //        category_id = 1;
            //        int patternNumber = getfileNamePattern();
            //       fileName = QString("PATTERN_%1").arg(patternNumber);
            //       fileName = QString("%1-%2#%3_Pa%4").arg(towerAndDistance->substation).arg(towerAndDistance->direction).arg(towerAndDistance->linenumber).arg(TimestampuseinFile);
            //       qDebug()<<"PATTERN_%1"<<fileName;
        }
    }
    else if(modeName == "Surge"){
        category_id = 4;
        fileNames = QString("%1_%2kV-%3#%4_S%5").arg(SetupEquipment->SubstationName).arg(SetupEquipment->Voltage).arg(SetupEquipment->TransmissionLineName).arg(SetupEquipment->LFLSerialNo).arg(TimestampuseinFile);
        // qWarning() << "modeName Surge::" << modeName << " fileName::" << fileName
        //            << " SetupEquipment->SubstationName" << SetupEquipment->SubstationName << " SetupEquipment->Voltage" << SetupEquipment->Voltage;
    }
    // qWarning() << "fileName:::" << fileNames
    //            << " TimestampuseinFile:::" << TimestampuseinFile
    //            << " modeName" << modeName;
    fileNames = safeEventFileName(fileNames, QStringLiteral("event.csv"));

    // qWarning() << "fileName:::" << fileNames
    //          << " TimestampuseinFile:::" << TimestampuseinFile
    //          << " modeName" << modeName;

    QString fullPath = dir.filePath(fileNames);
    fullPath = cleanAbsolutePath(fullPath);
    if (!isInsideAllowedEventWritePath(fullPath)) {
        qWarning() << "[SumNormalizationandUpdateDb] unsafe fullPath, skip:" << fullPath;
        return;
    }
    //    qDebug() << "fileName:"<<fullPath;
    //    QMap<QString, QJsonArray> distMap,voltMap;

    // qWarning() << "debug distMap" << distMap.contains("phaseA") << distMap.contains("phaseB") << distMap.contains("phaseC") << "\n"
    //          << "debug voltMap" << voltMap.contains("phaseA") << voltMap.contains("phaseB") << voltMap.contains("phaseC");
    if (modeName == "Surge") {
        qDebug() << "Surge writeCSV: and saveCsvFileAndUpdateDb phaseA";
        writeCSV(modeName, fileNames, fullPath, distMap["phaseA"], voltMap["phaseA"], distMap["phaseB"], voltMap["phaseB"], distMap["phaseC"], voltMap["phaseC"]);
        saveCsvFileAndUpdateDb(category_id, saveTimeInMysql, fileNames, fullPath);
        {
            const QFileInfo eventInfo(fullPath);
            eventCsvReady = eventInfo.exists() && eventInfo.isFile() &&
                            !eventInfo.isSymLink() && eventInfo.size() > 0;
        }
        distMap.clear();
        voltMap.clear();
    } else if (distMap.contains("phaseA") && distMap.contains("phaseB") && distMap.contains("phaseC") && voltMap.contains("phaseA") && voltMap.contains("phaseB") && voltMap.contains("phaseC")) {
        if (modeName == "Pattern") {
            PatterndistMap.clear();
            PatternvoltMap.clear();

            //            distAPat = distMap["phaseA"];
            //            voltAPat = voltMap["phaseA"];
            //            distBPat = distMap["phaseB"];
            //            voltBPat = voltMap["phaseB"];
            //            distCPat = distMap["phaseC"];
            //            voltCPat = voltMap["phaseC"];

            PatterndistMap["phaseA"] = distMap["phaseA"];
            PatternvoltMap["phaseA"] = voltMap["phaseA"];
            PatterndistMap["phaseB"] = distMap["phaseB"];
            PatternvoltMap["phaseB"] = voltMap["phaseB"];
            PatterndistMap["phaseC"] = distMap["phaseC"];
            PatternvoltMap["phaseC"] = voltMap["phaseC"];

            distMap.clear();
            voltMap.clear();
        } else if (modeName != "Pattern" && modeName != "Surge") {
            // qWarning() << "writeCSV: and saveCsvFileAndUpdateDb modeName==" << modeName << fullPath << fileNames;
            writeCSV(modeName, fileNames, fullPath, distMap["phaseA"], voltMap["phaseA"], distMap["phaseB"], voltMap["phaseB"], distMap["phaseC"], voltMap["phaseC"]);
            saveCsvFileAndUpdateDb(category_id, saveTimeInMysql, fileNames, fullPath);
            {
                const QFileInfo eventInfo(fullPath);
                eventCsvReady = eventInfo.exists() && eventInfo.isFile() &&
                                !eventInfo.isSymLink() && eventInfo.size() > 0;
            }
            distMap.clear();
            voltMap.clear();
        }
    }
    if (modeName != "Pattern" && eventCsvReady) {
        emit assignPATHCSV(fileNames, fullPath);
    } else if (modeName != "Pattern") {
        qDebug() << "[SumNormalizationandUpdateDb] event CSV is not physically ready;"
                   << "do not publish path yet:"
                   << "mode=" << modeName
                   << "phase=" << phase
                   << "path=" << fullPath;
    }
}


QVector<QJsonArray> splitByCycle(const QJsonArray& distance, const QJsonArray& voltage) {
    QVector<QJsonArray> result;
    QJsonArray currentCycle;
    if (distance.isEmpty() || voltage.isEmpty()) return result;

    currentCycle.append(QJsonObject{{"d", distance[0]}, {"v", voltage[0]}});
    for (int i = 1; i < distance.size(); ++i) {
        double prev = distance[i - 1].toDouble();
        double curr = distance[i].toDouble();
        if (curr < prev) {  // start of new cycle
            result.append(currentCycle);
            currentCycle = QJsonArray();
        }
        currentCycle.append(QJsonObject{{"d", curr}, {"v", voltage[i]}});
    }
    result.append(currentCycle);
    return result;
}

QJsonArray averageCycles(const QVector<QJsonArray>& cycles, const QString& key) {
    int maxLen = 0;
    for (const auto& cycle : cycles)
        maxLen = qMax(maxLen, cycle.size());

    QJsonArray result;
    for (int i = 0; i < maxLen; ++i) {
        double sum = 0;
        int count = 0;
        for (const auto& cycle : cycles) {
            if (i < cycle.size())
                sum += cycle[i].toObject()[key].toDouble();
            else
                sum += cycle.last().toObject()[key].toDouble(); // pad ด้วยค่าสุดท้าย
            ++count;
        }
        result.append(sum / count);
    }
    return result;
}

void Database::writeMarginCSV(QString path,QString countMarginA, QString countMarginB, QString countMarginC, const QJsonArray& marginAItems, const QJsonArray& marginBItems, const QJsonArray& marginCItems){
    path = cleanAbsolutePath(path);
    qDebug() << "writeMarginCSV:" << path;

    if (!isInsideAllowedEventWritePath(path)) {
        qWarning() << "[writeMarginCSV] unsafe path, skip:" << path;
        return;
    }

    if (!ensureDirExists(QFileInfo(path).absolutePath())) {
        qWarning() << "[writeMarginCSV] cannot create directory:" << QFileInfo(path).absolutePath();
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning("Unable to open file for writing.");
        return;
    }
    QTextStream out(&file);

    // Header
    out << "countMarginA,marginAItems(mV),countMarginB,marginBItems(mV),countMarginC,marginCItems(mV)\n";

    int maxSize = qMax(marginAItems.size(),
                       qMax(marginBItems.size(), marginCItems.size()));

    bool temp = false;

    for (int i = 0; i < maxSize; ++i) {
        QString colA, colB, colC;

        if (i < marginAItems.size() && marginAItems[i].isObject()) {
            QJsonObject obj = marginAItems[i].toObject();
            colA = QString::number(obj.value("valueMarginA").toInt());
        } else {
            colA = "";  // เว้นว่างถ้าไม่มี
        }

        if (i < marginBItems.size() && marginBItems[i].isObject()) {
            QJsonObject obj = marginBItems[i].toObject();
            colB = QString::number(obj.value("valueMarginB").toInt());
        } else {
            colB = "";
        }

        if (i < marginCItems.size() && marginCItems[i].isObject()) {
            QJsonObject obj = marginCItems[i].toObject();
            colC = QString::number(obj.value("valueMarginC").toInt());
        } else {
            colC = "";
        }

        // เขียนแถว: countMargin*, value*
        qDebug() << "เขียนแถว: countMargin*, value* toInt()::" << countMarginA.toInt() << countMarginB.toInt() << countMarginC.toInt()
                 << "เขียนแถว: countMargin*, value*::" << countMarginA << countMarginB << countMarginC;
        out << (temp == false ? countMarginA : "") << "," << colA << ","
            << (temp == false ? countMarginB : "") << "," << colB << ","
            << (temp == false ? countMarginC : "") << "," << colC << "\n";

        temp = true;
    }

    file.close();

    temp = false;
}

// void Database::writeCSV(QString modeName, const QString &fileName, const QString &FullName,
//                         const QJsonArray &distA, const QJsonArray &voltA,
//                         const QJsonArray &distB, const QJsonArray &voltB,
//                         const QJsonArray &distC, const QJsonArray &voltC)
// {
//     qDebug() << "write success:" << FullName;

//     QFile file(FullName);
//     if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
//         qWarning("Unable to open file for writing.");
//         return;
//     }
//     QTextStream out(&file);
//     out << "DistancePhaseA(km),VoltageA(mV),DistancePhaseB(km),VoltageB(mV),DistancePhaseC(km),VoltageC(mV)\n";

//     // --- แยกรอบ ---
//     auto cyclesA = splitByCycle(distA, voltA);
//     auto cyclesB = splitByCycle(distB, voltB);
//     auto cyclesC = splitByCycle(distC, voltC);

//     qDebug() << "Detected cycles: A=" << cyclesA.size()
//              << ", B=" << cyclesB.size()
//              << ", C=" << cyclesC.size();

//     // --- เฉลี่ยแต่ละรอบ ---
//     QJsonArray distAAvg = averageCycles(cyclesA, "d");
//     QJsonArray voltAAvg = averageCycles(cyclesA, "v");
//     QJsonArray distBAvg = averageCycles(cyclesB, "d");
//     QJsonArray voltBAvg = averageCycles(cyclesB, "v");
//     QJsonArray distCAvg = averageCycles(cyclesC, "d");
//     QJsonArray voltCAvg = averageCycles(cyclesC, "v");

//     const bool isSurge = (modeName.compare("Surge", Qt::CaseInsensitive) == 0);

//     // --- คำนวณความยาวสูงสุดจากเฟสที่มีข้อมูลจริง ---
//     int maxSize = 0;
//     maxSize = qMax(maxSize, distAAvg.size());
//     maxSize = qMax(maxSize, distBAvg.size());
//     maxSize = qMax(maxSize, distCAvg.size());
//     maxSize = qMax(maxSize, voltAAvg.size());
//     maxSize = qMax(maxSize, voltBAvg.size());
//     maxSize = qMax(maxSize, voltCAvg.size());

//     // ถ้าไม่มีข้อมูลเลยทุกเฟส ก็ไม่ต้องเขียนอะไรต่อ
//     if (maxSize <= 0) {
//         qWarning() << "[writeCSV] No data to write.";
//         file.close();
//         return;
//     }

//     // --- ฟังก์ชันช่วย pad/extend ---
//     auto padArrayTo = [](QJsonArray& array, int targetSize) {
//         if (array.isEmpty()) return; // ไม่ทำอะไรตรงนี้ (ไปเติมในขั้น Surge แทน)
//         QJsonValue lastVal = array.last();
//         while (array.size() < targetSize)
//             array.append(lastVal);
//     };

//     // --- ถ้าไม่ใช่ Surge => พฤติกรรมเดิม: pad ด้วยค่า last ของแต่ละเฟสที่มี ---
//     if (!isSurge) {
//         padArrayTo(distAAvg, maxSize);
//         padArrayTo(voltAAvg, maxSize);
//         padArrayTo(distBAvg, maxSize);
//         padArrayTo(voltBAvg, maxSize);
//         padArrayTo(distCAvg, maxSize);
//         padArrayTo(voltCAvg, maxSize);
//     } else {
//         // --- ถ้าเป็น Surge => เฟสที่ "ไม่มีข้อมูลเลย" ให้เติม 0 จนเท่ากับ maxSize ---
//         auto fillZeroTo = [](QJsonArray& array, int targetSize) {
//             while (array.size() < targetSize) array.append(0.0);
//         };
//         // กรณีมีข้อมูลบ้างแต่สั้นกว่า ก็ pad ด้วยค่าท้าย (พฤติกรรมเดิม)
//         padArrayTo(distAAvg, maxSize);
//         padArrayTo(voltAAvg, maxSize);
//         padArrayTo(distBAvg, maxSize);
//         padArrayTo(voltBAvg, maxSize);
//         padArrayTo(distCAvg, maxSize);
//         padArrayTo(voltCAvg, maxSize);
//         // กรณีว่างทั้งอาร์เรย์ -> เติมศูนย์
//         if (distAAvg.isEmpty()) fillZeroTo(distAAvg, maxSize);
//         if (voltAAvg.isEmpty()) fillZeroTo(voltAAvg, maxSize);
//         if (distBAvg.isEmpty()) fillZeroTo(distBAvg, maxSize);
//         if (voltBAvg.isEmpty()) fillZeroTo(voltBAvg, maxSize);
//         if (distCAvg.isEmpty()) fillZeroTo(distCAvg, maxSize);
//         if (voltCAvg.isEmpty()) fillZeroTo(voltCAvg, maxSize);
//     }

//     // --- helper: อ่านค่า index i อย่างปลอดภัย พร้อม default ---
//     auto getOrDefault = [](const QJsonArray& arr, int i, double defVal) -> double {
//         if (i >= 0 && i < arr.size()) return arr[i].toDouble(defVal);
//         return defVal;
//     };

//     // --- เขียนข้อมูล ---
//     for (int i = 0; i < maxSize; ++i) {
//         // ถ้า Surge ให้ default เป็น 0; ถ้าไม่ Surge ให้ default เป็นค่าท้ายของแต่ละเฟส (ถ้ามี) หรือ 0
//         // แต่เรา pad/deduct ไปแล้วด้านบน จึงใช้ 0 เป็น safety เฉย ๆ
//         const double aD = getOrDefault(distAAvg, i, 0.0);
//         const double aV = getOrDefault(voltAAvg, i, 0.0);
//         const double bD = getOrDefault(distBAvg, i, isSurge ? 0.0 : 0.0);
//         const double bV = getOrDefault(voltBAvg, i, isSurge ? 0.0 : 0.0);
//         const double cD = getOrDefault(distCAvg, i, isSurge ? 0.0 : 0.0);
//         const double cV = getOrDefault(voltCAvg, i, isSurge ? 0.0 : 0.0);

//         out << QString::number(aD) << "," << QString::number(aV) << ","
//             << QString::number(bD) << "," << QString::number(bV) << ","
//             << QString::number(cD) << "," << QString::number(cV) << "\n";
//     }

//     file.close();
//     qDebug() << "write success: mode" << modeName;

//     // ======= ส่วน FTP/อัพโหลดคงเดิมด้านล่าง =======
//     QString sanitizedFileName = QFileInfo(fileName).fileName().replace("#", "");
//     QString ftpUrl, ftpUrlNew;
//     QString ftpUrlFolder, ftpUrlFolder2, ftpPattern;
//     qDebug() << "fileName" << fileName << "fileName" << sanitizedFileName;
//     QThread::msleep(100);

//     if (modeName == "Surge") {
//         ftpUrl = QString("ftp://%1/%2/%3/%4/%5").arg(FTP_Param_->FTP_IP).arg(ftpSurgeRoot).arg(ftpDate).arg(ftpTime).arg(sanitizedFileName);
//         ftpUrlNew = QString("lftp -u %1,%2 %3 -e 'set net:timeout 5; set net:reconnect-interval-base 1; set net:max-retries 1; cd %4/%5/%6/%7/%8/%9; put %10/%11/%12/%13 -o %13.csv bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpSurgeRoot).arg(ftpSubstation).arg(ftpLine).arg("Surge").arg(ftpDate).arg(ftpTime).arg(SURGE_PATH).arg(ftpDate).arg(ftpTime).arg(sanitizedFileName);
//         ftpUrlFolder = QString("lftp -u %1,%2 %3 -e 'set net:timeout 3; set net:reconnect-interval-base 1; set net:max-retries 1; mkdir %4/%5; mkdir %4/%5/%6; mkdir %4/%5/%6/%7; mkdir %4/%5/%6/%7/%8; bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpSurgeRoot).arg(ftpSubstation).arg(ftpLine).arg("Surge").arg(ftpDate);
//         ftpUrlFolder2 = QString("lftp -u %1,%2 %3 -e 'set net:timeout 3; set net:reconnect-interval-base 1; set net:max-retries 1; mkdir %4/%5/%6/%7/%8/%9; bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpSurgeRoot).arg(ftpSubstation).arg(ftpLine).arg("Surge").arg(ftpDate).arg(ftpTime);
//         ftpPattern = QString("lftp -u %1,%2 %3 -e 'set net:timeout 5; set net:reconnect-interval-base 1; set net:max-retries 1; cd %4/%5/%6/%7/%8/%9; put %10 -o %11.csv bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpSurgeRoot).arg(ftpSubstation).arg(ftpLine).arg("Surge").arg(ftpDate).arg(ftpTime).arg(selectPatterPath).arg(safePatternName);
//         fullPathPattern = QString("%1/%2/%3/%4/%5/%6/%7").arg(ftpSurgeRoot).arg(ftpSubstation).arg(ftpLine).arg("Surge").arg(ftpDate).arg(ftpTime).arg(safePatternName);
//         fullPicPATH = QString("%1/%2/%3/%4/%5/%6/").arg(ftpSurgeRoot).arg(ftpSubstation).arg(ftpLine).arg("Surge").arg(ftpDate).arg(ftpTime);
//         emit startFtpTimer(ftpUrlFolder.toUtf8(), ftpUrlFolder2.toUtf8(), ftpPattern.toUtf8());
//     } else if (modeName == "Relay") {
//         ftpUrl = QString("ftp://%1/%2/%3/%4/%5").arg(FTP_Param_->FTP_IP).arg(ftpRelayRoot).arg(ftpDate).arg(ftpTime).arg(sanitizedFileName);
//         ftpUrlNew = QString("lftp -u %1,%2 %3 -e 'set net:timeout 5; set net:reconnect-interval-base 1; set net:max-retries 1; cd %4/%5/%6/%7/%8/%9; put %10/%11/%12/%13 -o %13.csv bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpRelayRoot).arg(ftpSubstation).arg(ftpLine).arg("Relay").arg(ftpDate).arg(ftpTime).arg(RELAY_PATH).arg(ftpDate).arg(ftpTime).arg(sanitizedFileName);
//         ftpUrlFolder = QString("lftp -u %1,%2 %3 -e 'set net:timeout 3; set net:reconnect-interval-base 1; set net:max-retries 1; mkdir %4/%5; mkdir %4/%5/%6; mkdir %4/%5/%6/%7; mkdir %4/%5/%6/%7/%8; bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpRelayRoot).arg(ftpSubstation).arg(ftpLine).arg("Relay").arg(ftpDate);
//         qDebug() << "cmd 1 ftpUrlFolder" << ftpUrlFolder;
//         ftpUrlFolder2 = QString("lftp -u %1,%2 %3 -e 'set net:timeout 3; set net:reconnect-interval-base 1; set net:max-retries 1; mkdir %4/%5/%6/%7/%8/%9; bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpRelayRoot).arg(ftpSubstation).arg(ftpLine).arg("Relay").arg(ftpDate).arg(ftpTime);
//         ftpPattern = QString("lftp -u %1,%2 %3 -e 'set net:timeout 5; set net:reconnect-interval-base 1; set net:max-retries 1; cd %4/%5/%6/%7/%8/%9; put %10 -o %11.csv bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpRelayRoot).arg(ftpSubstation).arg(ftpLine).arg("Relay").arg(ftpDate).arg(ftpTime).arg(selectPatterPath).arg(safePatternName);
//         fullPathPattern = QString("%1/%2/%3/%4/%5/%6/%7").arg(ftpRelayRoot).arg(ftpSubstation).arg(ftpLine).arg("Relay").arg(ftpDate).arg(ftpTime).arg(safePatternName);
//         fullPicPATH = QString("%1/%2/%3/%4/%5/%6/").arg(ftpSurgeRoot).arg(ftpSubstation).arg(ftpLine).arg("Relay").arg(ftpDate).arg(ftpTime);
//         qDebug() << "cmd 2 ftpUrlFolder" << ftpUrlFolder2;
//         emit startFtpTimer(ftpUrlFolder.toUtf8(), ftpUrlFolder2.toUtf8(), ftpPattern.toUtf8());
//     } else if (modeName == "Periodic") {
//         ftpUrl = QString("ftp://%1/%2/%3/%4/%5").arg(FTP_Param_->FTP_IP).arg(ftpPeriodicRoot).arg(ftpDate).arg(ftpTime).arg(sanitizedFileName);
//         ftpUrlNew = QString("lftp -u %1,%2 %3 -e 'set net:timeout 5; set net:reconnect-interval-base 1; set net:max-retries 1; cd %4/%5/%6/%7/%8/%9; put %10/%11/%12/%13 -o %13.csv bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpPeriodicRoot).arg(ftpSubstation).arg(ftpLine).arg("Periodic").arg(ftpDate).arg(ftpTime).arg(PERIODIC_PATH).arg(ftpDate).arg(ftpTime).arg(sanitizedFileName);
//         ftpUrlFolder = QString("lftp -u %1,%2 %3 -e 'set net:timeout 3; set net:reconnect-interval-base 1; set net:max-retries 1; mkdir %4/%5; mkdir %4/%5/%6; mkdir %4/%5/%6/%7; mkdir %4/%5/%6/%7/%8; bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpPeriodicRoot).arg(ftpSubstation).arg(ftpLine).arg("Periodic").arg(ftpDate);
//         ftpUrlFolder2 = QString("lftp -u %1,%2 %3 -e 'set net:timeout 3; set net:reconnect-interval-base 1; set net:max-retries 1; mkdir %4/%5/%6/%7/%8/%9; bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpPeriodicRoot).arg(ftpSubstation).arg(ftpLine).arg("Periodic").arg(ftpDate).arg(ftpTime);
//         ftpPattern = QString("lftp -u %1,%2 %3 -e 'set net:timeout 5; set net:reconnect-interval-base 1; set net:max-retries 1; cd %4/%5/%6/%7/%8/%9; put %10 -o %11.csv bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpPeriodicRoot).arg(ftpSubstation).arg(ftpLine).arg("Periodic").arg(ftpDate).arg(ftpTime).arg(selectPatterPath).arg(safePatternName);
//         fullPathPattern = QString("%1/%2/%3/%4/%5/%6/%7").arg(ftpPeriodicRoot).arg(ftpSubstation).arg(ftpLine).arg("Periodic").arg(ftpDate).arg(ftpTime).arg(safePatternName);
//         fullPicPATH = QString("%1/%2/%3/%4/%5/%6/").arg(ftpSurgeRoot).arg(ftpSubstation).arg(ftpLine).arg("Periodic").arg(ftpDate).arg(ftpTime);
//         //        qDebug() << "cmd 1 ftpUrlFolder" << ftpUrlFolder;
//         //        qDebug() << "cmd 2 ftpUrlFolder" << ftpUrlFolder2;
//         qDebug() << "already capture fullPathPattern:" << fullPathPattern
//                  << " ftpUrlFolder:" << ftpUrlFolder
//                  << " ftpUrlFolder2:" << ftpUrlFolder2
//                  << " ftpPattern:" << ftpPattern;
//         emit startFtpTimer(ftpUrlFolder.toUtf8(), ftpUrlFolder2.toUtf8(), ftpPattern.toUtf8());
//     }else if (modeName == "Pattern") {
//         auto findCycleIndices = [](const QJsonArray& distArray) {
//             QList<int> indices;
//             indices << 0;
//             for (int i = 1; i < distArray.size(); ++i) {
//                 if (distArray[i].toDouble() < distArray[i - 1].toDouble())
//                     indices << i;
//             }
//             indices << distArray.size();
//             return indices;
//         };

//         auto extractCycle = [](const QJsonArray& arr, int start, int end) {
//             QVector<double> result;
//             for (int i = start; i < end; ++i)
//                 result.append(arr[i].toDouble());
//             return result;
//         };

//         auto extendToSize = [](QVector<double>& vec, int targetSize) {
//             if (vec.isEmpty()) return;
//             double last = vec.last();
//             while (vec.size() < targetSize)
//                 vec.append(last);
//         };

//         QList<int> breaksA = findCycleIndices(distA);
//         QList<int> breaksB = findCycleIndices(distB);
//         QList<int> breaksC = findCycleIndices(distC);
//         int cycleCount = std::min({ breaksA.size() - 1, breaksB.size() - 1, breaksC.size() - 1 });

//         QVector<QVector<double>> allDistA, allVoltA, allDistB, allVoltB, allDistC, allVoltC;
//         QVector<int> cycleLens;

//         for (int i = 0; i < cycleCount; ++i) {
//             QVector<double> da = extractCycle(distA, breaksA[i], breaksA[i + 1]);
//             QVector<double> va = extractCycle(voltA, breaksA[i], breaksA[i + 1]);
//             QVector<double> db = extractCycle(distB, breaksB[i], breaksB[i + 1]);
//             QVector<double> vb = extractCycle(voltB, breaksB[i], breaksB[i + 1]);
//             QVector<double> dc = extractCycle(distC, breaksC[i], breaksC[i + 1]);
//             QVector<double> vc = extractCycle(voltC, breaksC[i], breaksC[i + 1]);

//             int maxLen = std::max({ da.size(), db.size(), dc.size() });

//             extendToSize(da, maxLen);
//             extendToSize(va, maxLen);
//             extendToSize(db, maxLen);
//             extendToSize(vb, maxLen);
//             extendToSize(dc, maxLen);
//             extendToSize(vc, maxLen);

//             allDistA << da; allVoltA << va;
//             allDistB << db; allVoltB << vb;
//             allDistC << dc; allVoltC << vc;

//             cycleLens << maxLen;
//             qDebug() << QString("[Cycle %1] Lengths => A:%2, B:%3, C:%4, Final:%5")
//                             .arg(i + 1)
//                             .arg(da.size()).arg(db.size()).arg(dc.size()).arg(maxLen);

//             qDebug() << "allDistA:" << allDistA << "allDistB:" << allDistB << "allDistC:" << allDistC;
//         }

//         int finalLen = *std::max_element(cycleLens.begin(), cycleLens.end());
//         QJsonArray avgDistA, avgVoltA, avgDistB, avgVoltB, avgDistC, avgVoltC;

//         for (int i = 0; i < finalLen; ++i) {
//             double sum = 0;

//             sum = 0; for (const auto& vec : allDistA) if (i < vec.size()) sum += vec[i];
//             avgDistA.append(sum / cycleCount);

//             sum = 0; for (const auto& vec : allVoltA) if (i < vec.size()) sum += vec[i];
//             avgVoltA.append(sum / cycleCount);

//             sum = 0; for (const auto& vec : allDistB) if (i < vec.size()) sum += vec[i];
//             avgDistB.append(sum / cycleCount);

//             sum = 0; for (const auto& vec : allVoltB) if (i < vec.size()) sum += vec[i];
//             avgVoltB.append(sum / cycleCount);

//             sum = 0; for (const auto& vec : allDistC) if (i < vec.size()) sum += vec[i];
//             avgDistC.append(sum / cycleCount);

//             sum = 0; for (const auto& vec : allVoltC) if (i < vec.size()) sum += vec[i];
//             avgVoltC.append(sum / cycleCount);
//         }

//         qDebug() << "[Pattern] Final averaged length:" << finalLen;

//         // Save to CSV
//         QString csvPath = QString("%1/%2/%3.csv").arg(PATTERN_PATH).arg(ftpDate).arg(ftpTime);
//         QFile file(csvPath);
//         if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
//             QTextStream out(&file);
//             out << "DistancePhaseA(km),VoltageA(mV),DistancePhaseB(km),VoltageB(mV),DistancePhaseC(km),VoltageC(mV)\n";
//             for (int i = 0; i < finalLen; ++i) {
//                 out << avgDistA[i].toDouble() << "," << avgVoltA[i].toDouble() << ","
//                     << avgDistB[i].toDouble() << "," << avgVoltB[i].toDouble() << ","
//                     << avgDistC[i].toDouble() << "," << avgVoltC[i].toDouble() << "\n";
//             }
//             file.close();
//         }

//         // Send via FTP
//         QString patternCsvName = QString("%1.csv").arg(ftpTime);
//         ftpPattern = QString("lftp -u %1,%2 %3 -e 'set net:timeout 5; set net:reconnect-interval-base 1; set net:max-retries 1; cd %4/%5/%6/%7/%8/%9; put %10 -o %11 bye'")
//                          .arg(FTP_Param_->USERNAME)
//                          .arg(FTP_Param_->PASSWORD)
//                          .arg(FTP_Param_->FTP_IP)
//                          .arg(ftpPatternRoot)
//                          .arg(ftpSubstation)
//                          .arg(ftpLine)
//                          .arg("Pattern")
//                          .arg(ftpDate)
//                          .arg(ftpTime)
//                          .arg(csvPath)
//                          .arg(patternCsvName);

//         qDebug() << "[Pattern] ftpPattern command:" << ftpPattern;
//         emit startFtpTimer(ftpUrlFolder.toUtf8(), ftpUrlFolder2.toUtf8(), ftpPattern.toUtf8());
//     }else {
//         ftpUrlNew = "";
//         ftpUrl = "";
//     }

//     emit updatePATHEmail(fullPathPattern, fullPicPATH);
//     qDebug() << "DEBUGmodeNameinFTPFUNCTION" << modeName << ftpUrl;
//     if (!ftpUrlNew.isEmpty()) {
//         emit uploadFTP(ftpUrlNew.toUtf8());
//     }
//     emit uploadToSNMP(FullName, fileName);
//     qDebug() << "fuhiuidfjuifhdfuiijfhdudfuihdfiuhifhfhdfyuerwgtfgewyugsfyugwefuygewyugf"
//              << ftpUrl << "fileName" << fileName;
// }

static QString buildFtpEventDir(const QString &root,
                                const QString &substation,
                                const QString &line,
                                const QString &mode,
                                const QString &date,
                                const QString &time)
{
    QString path = QString("%1/%2/%3/%4/%5/%6")
    .arg(root)
        .arg(substation)
        .arg(line)
        .arg(mode)
        .arg(date)
        .arg(time);

    path.replace(QRegularExpression("/{2,}"), "/");

    if (!path.endsWith("/")) {
        path += "/";
    }

    return path;
}

void Database::writeCSV(QString modeName, const QString &fileName, const QString &FullName, const QJsonArray &distA, const QJsonArray &voltA, const QJsonArray &distB, const QJsonArray &voltB, const QJsonArray &distC, const QJsonArray &voltC) {
    // qWarning() << "write success:" << FullName;

    QFile file(FullName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning("Unable to open file for writing.");
        // qWarning() << "FullName" << FullName;
        return;
    }
    QTextStream out(&file);
    out << "DistancePhaseA(km),VoltageA(mV),DistancePhaseB(km),VoltageB(mV),DistancePhaseC(km),VoltageC(mV)\n";

    // แยกรอบ
    auto cyclesA = splitByCycle(distA, voltA);
    auto cyclesB = splitByCycle(distB, voltB);
    auto cyclesC = splitByCycle(distC, voltC);

    // qWarning() << "Detected cycles: A=" << cyclesA.size() << ", B=" << cyclesB.size() << ", C=" << cyclesC.size();

    // เฉลี่ยแต่ละรอบ
    QJsonArray distAAvg = averageCycles(cyclesA, "d");
    QJsonArray voltAAvg = averageCycles(cyclesA, "v");
    QJsonArray distBAvg = averageCycles(cyclesB, "d");
    QJsonArray voltBAvg = averageCycles(cyclesB, "v");
    QJsonArray distCAvg = averageCycles(cyclesC, "d");
    QJsonArray voltCAvg = averageCycles(cyclesC, "v");

    // Padding ให้เท่ากัน
    int maxSize = qMax(distAAvg.size(), qMax(distBAvg.size(), distCAvg.size()));

    auto padArray = [](QJsonArray& array, int targetSize) {
        if (array.isEmpty()) return;
        QJsonValue lastVal = array.last();
        while (array.size() < targetSize)
            array.append(lastVal);
    };

    padArray(distAAvg, maxSize);
    padArray(voltAAvg, maxSize);
    padArray(distBAvg, maxSize);
    padArray(voltBAvg, maxSize);
    padArray(distCAvg, maxSize);
    padArray(voltCAvg, maxSize);

    // เขียนข้อมูล
    for (int i = 0; i < maxSize; ++i) {
        QString row = QString("%1,%2,%3,%4,%5,%6")
        .arg(distAAvg[i].toDouble())
            .arg(voltAAvg[i].toDouble())
            .arg(distBAvg[i].toDouble())
            .arg(voltBAvg[i].toDouble())
            .arg(distCAvg[i].toDouble())
            .arg(voltCAvg[i].toDouble());
        out << row << "\n";
    }

    file.close();
    // qWarning() << "write success: mode" << modeName;


    QString sanitizedFileName = safeEventFileName(fileName, QStringLiteral("event.csv"));
    const QString sanitizedBaseName = QFileInfo(sanitizedFileName).completeBaseName();
    Q_UNUSED(sanitizedBaseName);
    const QString ftpSubstation = safePathPart(SetupEquipment->SubstationName, QStringLiteral("Substation"));
    const QString ftpLine = safePathPart(SetupEquipment->TransmissionLineName, QStringLiteral("Line"));
    const QString ftpDate = safePathPart(DateKept, QStringLiteral("Date"));
    const QString ftpTime = safePathPart(TimeKept, QStringLiteral("Time"));
    const QString ftpSurgeRoot = safeFtpRootPath(FTP_Param_->SURGE_FILE, QStringLiteral("Surge"));
    const QString ftpRelayRoot = safeFtpRootPath(FTP_Param_->RELAY_FILE, QStringLiteral("Relay"));
    const QString ftpManualRoot = safeFtpRootPath(FTP_Param_->MANUAL_FILE, QStringLiteral("Manual"));
    const QString ftpPeriodicRoot = safeFtpRootPath(FTP_Param_->PERIODIC_FILE, QStringLiteral("Periodic"));
    const QString ftpPatternRoot = safeFtpRootPath(FTP_Param_->PATTERN_FILE, QStringLiteral("Pattern"));
    const QString safePatternName = safeEventFileName(selectPatterName, QStringLiteral("pattern.csv"));
    QString ftpUrl, ftpUrlNew;
    QString ftpUrlFolder, ftpUrlFolder2, ftpPattern;
    fullPathPattern.clear();
    fullPicPATH.clear();

    const QString eventMode = canonicalEventCategory(modeName);
    if (eventMode.isEmpty()) {
        qWarning() << "[writeCSV] invalid modeName, stop FTP/email path update:"
                   << "modeName =" << modeName;
        return;
    }
    // qWarning()<<"fileName"<<fileName<<"sanitizedFileName"<<sanitizedFileName;
    //     selectPatter
    QThread::msleep(100);
    if (eventMode == "Surge") {
        ftpUrl = QString("ftp://%1/%2/%3/%4/%5").arg(FTP_Param_->FTP_IP).arg(ftpSurgeRoot).arg(ftpDate).arg(ftpTime).arg(sanitizedFileName);
        ftpUrlNew = QString("lftp -u %1,%2 %3 -e 'set net:timeout 5; set net:reconnect-interval-base 1; set net:max-retries 1; cd %4/%5/%6/%7/%8/%9; put %10/%11/%12/%13 -o %13.csv bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpSurgeRoot).arg(ftpSubstation).arg(ftpLine).arg("Surge").arg(ftpDate).arg(ftpTime).arg(SURGE_PATH).arg(ftpDate).arg(ftpTime).arg(sanitizedFileName);
        ftpUrlFolder = QString("lftp -u %1,%2 %3 -e 'set net:timeout 3; set net:reconnect-interval-base 1; set net:max-retries 1; mkdir %4/%5; mkdir %4/%5/%6; mkdir %4/%5/%6/%7; mkdir %4/%5/%6/%7/%8; bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpSurgeRoot).arg(ftpSubstation).arg(ftpLine).arg("Surge").arg(ftpDate);
        ftpUrlFolder2 = QString("lftp -u %1,%2 %3 -e 'set net:timeout 3; set net:reconnect-interval-base 1; set net:max-retries 1; mkdir %4/%5/%6/%7/%8/%9; bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpSurgeRoot).arg(ftpSubstation).arg(ftpLine).arg("Surge").arg(ftpDate).arg(ftpTime);
        ftpPattern = QString("lftp -u %1,%2 %3 -e 'set net:timeout 5; set net:reconnect-interval-base 1; set net:max-retries 1; cd %4/%5/%6/%7/%8/%9; put %10 -o %11.csv bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpSurgeRoot).arg(ftpSubstation).arg(ftpLine).arg("Surge").arg(ftpDate).arg(ftpTime).arg(selectPatterPath).arg(safePatternName);
        fullPathPattern = QString("%1/%2/%3/%4/%5/%6/%7").arg(ftpSurgeRoot).arg(ftpSubstation).arg(ftpLine).arg("Surge").arg(ftpDate).arg(ftpTime).arg(safePatternName);
        fullPicPATH = buildFtpEventDir(
            ftpSurgeRoot,
            ftpSubstation,
            ftpLine,
            QStringLiteral("Surge"),
            ftpDate,
            ftpTime
            );
        qDebug() << "[EVENT-FTP][DEFERRED] Event FTP waits for mandatory Picture + Event CSV gate:"
                 << "mode=" << eventMode
                 << "date=" << ftpDate
                 << "time=" << ftpTime;
    } else if (eventMode == "Relay") {
        ftpUrl = QString("ftp://%1/%2/%3/%4/%5").arg(FTP_Param_->FTP_IP).arg(ftpRelayRoot).arg(ftpDate).arg(ftpTime).arg(sanitizedFileName);
        ftpUrlNew = QString("lftp -u %1,%2 %3 -e 'set net:timeout 5; set net:reconnect-interval-base 1; set net:max-retries 1; cd %4/%5/%6/%7/%8/%9; put %10/%11/%12/%13 -o %13.csv bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpRelayRoot).arg(ftpSubstation).arg(ftpLine).arg("Relay").arg(ftpDate).arg(ftpTime).arg(RELAY_PATH).arg(ftpDate).arg(ftpTime).arg(sanitizedFileName);
        ftpUrlFolder = QString("lftp -u %1,%2 %3 -e 'set net:timeout 3; set net:reconnect-interval-base 1; set net:max-retries 1; mkdir %4/%5; mkdir %4/%5/%6; mkdir %4/%5/%6/%7; mkdir %4/%5/%6/%7/%8; bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpRelayRoot).arg(ftpSubstation).arg(ftpLine).arg("Relay").arg(ftpDate);
        qDebug() << "cmd 1 ftpUrlFolder" << ftpUrlFolder;
        ftpUrlFolder2 = QString("lftp -u %1,%2 %3 -e 'set net:timeout 3; set net:reconnect-interval-base 1; set net:max-retries 1; mkdir %4/%5/%6/%7/%8/%9; bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpRelayRoot).arg(ftpSubstation).arg(ftpLine).arg("Relay").arg(ftpDate).arg(ftpTime);
        ftpPattern = QString("lftp -u %1,%2 %3 -e 'set net:timeout 5; set net:reconnect-interval-base 1; set net:max-retries 1; cd %4/%5/%6/%7/%8/%9; put %10 -o %11.csv bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpRelayRoot).arg(ftpSubstation).arg(ftpLine).arg("Relay").arg(ftpDate).arg(ftpTime).arg(selectPatterPath).arg(safePatternName);
        fullPathPattern = QString("%1/%2/%3/%4/%5/%6/%7").arg(ftpRelayRoot).arg(ftpSubstation).arg(ftpLine).arg("Relay").arg(ftpDate).arg(ftpTime).arg(safePatternName);
        fullPicPATH = buildFtpEventDir(
            ftpRelayRoot,
            ftpSubstation,
            ftpLine,
            QStringLiteral("Relay"),
            ftpDate,
            ftpTime
            );
        qDebug() << "cmd 2 ftpUrlFolder" << ftpUrlFolder2;
        qDebug() << "[EVENT-FTP][DEFERRED] Event FTP waits for mandatory Picture + Event CSV gate:"
                 << "mode=" << eventMode
                 << "date=" << ftpDate
                 << "time=" << ftpTime;
    } else if (eventMode == "Manual") {
        ftpUrl = QString("ftp://%1/%2/%3/%4/%5").arg(FTP_Param_->FTP_IP).arg(ftpManualRoot).arg(ftpDate).arg(ftpTime).arg(sanitizedFileName);
        ftpUrlNew = QString("lftp -u %1,%2 %3 -e 'set net:timeout 5; set net:reconnect-interval-base 1; set net:max-retries 1; cd %4/%5/%6/%7/%8/%9; put %10/%11/%12/%13 -o %13.csv bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpManualRoot).arg(ftpSubstation).arg(ftpLine).arg("Manual").arg(ftpDate).arg(ftpTime).arg(MANUAL_PATH).arg(ftpDate).arg(ftpTime).arg(sanitizedFileName);
        ftpUrlFolder = QString("lftp -u %1,%2 %3 -e 'set net:timeout 3; set net:reconnect-interval-base 1; set net:max-retries 1; mkdir %4/%5; mkdir %4/%5/%6; mkdir %4/%5/%6/%7; mkdir %4/%5/%6/%7/%8; bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpManualRoot).arg(ftpSubstation).arg(ftpLine).arg("Manual").arg(ftpDate);
        // qWarning() << modeName << " Manual cmd 1 ftpUrlFolder" << ftpUrlFolder << " sanitizedFileName:"<< sanitizedFileName;
        ftpUrlFolder2 = QString("lftp -u %1,%2 %3 -e 'set net:timeout 3; set net:reconnect-interval-base 1; set net:max-retries 1; mkdir %4/%5/%6/%7/%8/%9; bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpManualRoot).arg(ftpSubstation).arg(ftpLine).arg("Manual").arg(ftpDate).arg(ftpTime);
        ftpPattern = QString("lftp -u %1,%2 %3 -e 'set net:timeout 5; set net:reconnect-interval-base 1; set net:max-retries 1; cd %4/%5/%6/%7/%8/%9; put %10 -o %11.csv bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpManualRoot).arg(ftpSubstation).arg(ftpLine).arg("Manual").arg(ftpDate).arg(ftpTime).arg(selectPatterPath).arg(safePatternName);
        fullPathPattern = QString("%1/%2/%3/%4/%5/%6/%7").arg(ftpManualRoot).arg(ftpSubstation).arg(ftpLine).arg("Manual").arg(ftpDate).arg(ftpTime).arg(safePatternName);
        fullPicPATH = buildFtpEventDir(
            ftpManualRoot,
            ftpSubstation,
            ftpLine,
            QStringLiteral("Manual"),
            ftpDate,
            ftpTime
            );
        // qWarning() << modeName << " Manual cmd 2 ftpUrlFolder" << ftpUrlFolder2;
        qDebug() << "[EVENT-FTP][DEFERRED] Event FTP waits for mandatory Picture + Event CSV gate:"
                 << "mode=" << eventMode
                 << "date=" << ftpDate
                 << "time=" << ftpTime;
    } else if (eventMode == "Periodic") {
        ftpUrl = QString("ftp://%1/%2/%3/%4/%5").arg(FTP_Param_->FTP_IP).arg(ftpPeriodicRoot).arg(ftpDate).arg(ftpTime).arg(sanitizedFileName);
        ftpUrlNew = QString("lftp -u %1,%2 %3 -e 'set net:timeout 5; set net:reconnect-interval-base 1; set net:max-retries 1; cd %4/%5/%6/%7/%8/%9; put %10/%11/%12/%13 -o %13.csv bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpPeriodicRoot).arg(ftpSubstation).arg(ftpLine).arg("Periodic").arg(ftpDate).arg(ftpTime).arg(PERIODIC_PATH).arg(ftpDate).arg(ftpTime).arg(sanitizedFileName);
        ftpUrlFolder = QString("lftp -u %1,%2 %3 -e 'set net:timeout 3; set net:reconnect-interval-base 1; set net:max-retries 1; mkdir %4/%5; mkdir %4/%5/%6; mkdir %4/%5/%6/%7; mkdir %4/%5/%6/%7/%8; bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpPeriodicRoot).arg(ftpSubstation).arg(ftpLine).arg("Periodic").arg(ftpDate);
        ftpUrlFolder2 = QString("lftp -u %1,%2 %3 -e 'set net:timeout 3; set net:reconnect-interval-base 1; set net:max-retries 1; mkdir %4/%5/%6/%7/%8/%9; bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpPeriodicRoot).arg(ftpSubstation).arg(ftpLine).arg("Periodic").arg(ftpDate).arg(ftpTime);
        ftpPattern = QString("lftp -u %1,%2 %3 -e 'set net:timeout 5; set net:reconnect-interval-base 1; set net:max-retries 1; cd %4/%5/%6/%7/%8/%9; put %10 -o %11.csv bye'").arg(FTP_Param_->USERNAME).arg(FTP_Param_->PASSWORD).arg(FTP_Param_->FTP_IP).arg(ftpPeriodicRoot).arg(ftpSubstation).arg(ftpLine).arg("Periodic").arg(ftpDate).arg(ftpTime).arg(selectPatterPath).arg(safePatternName);
        fullPathPattern = QString("%1/%2/%3/%4/%5/%6/%7").arg(ftpPeriodicRoot).arg(ftpSubstation).arg(ftpLine).arg("Periodic").arg(ftpDate).arg(ftpTime).arg(safePatternName);
        fullPicPATH = buildFtpEventDir(
            ftpPeriodicRoot,
            ftpSubstation,
            ftpLine,
            QStringLiteral("Periodic"),
            ftpDate,
            ftpTime
            );
        //        qDebug() << "cmd 1 ftpUrlFolder" << ftpUrlFolder;
        //        qDebug() << "cmd 2 ftpUrlFolder" << ftpUrlFolder2;
        qDebug() << "already createcsv"
                 << " ftpUrlFolder:" << ftpUrlFolder
                 << " ftpUrlFolder2:" << ftpUrlFolder2
                 << " ftpPattern:" << ftpPattern;
        qDebug() << "[EVENT-FTP][DEFERRED] Event FTP waits for mandatory Picture + Event CSV gate:"
                 << "mode=" << eventMode
                 << "date=" << ftpDate
                 << "time=" << ftpTime;
    } else if (eventMode == "Pattern") {
        auto findCycleIndices = [](const QJsonArray& distArray) {
            QList<int> indices;
            indices << 0;
            for (int i = 1; i < distArray.size(); ++i) {
                if (distArray[i].toDouble() < distArray[i - 1].toDouble())
                    indices << i;
            }
            indices << distArray.size();
            return indices;
        };

        auto extractCycle = [](const QJsonArray& arr, int start, int end) {
            QVector<double> result;
            for (int i = start; i < end; ++i)
                result.append(arr[i].toDouble());
            return result;
        };

        auto extendToSize = [](QVector<double>& vec, int targetSize) {
            if (vec.isEmpty()) return;
            double last = vec.last();
            while (vec.size() < targetSize)
                vec.append(last);
        };

        QList<int> breaksA = findCycleIndices(distA);
        QList<int> breaksB = findCycleIndices(distB);
        QList<int> breaksC = findCycleIndices(distC);
        int cycleCount = std::min({ breaksA.size() - 1, breaksB.size() - 1, breaksC.size() - 1 });

        QVector<QVector<double>> allDistA, allVoltA, allDistB, allVoltB, allDistC, allVoltC;
        QVector<int> cycleLens;

        for (int i = 0; i < cycleCount; ++i) {
            QVector<double> da = extractCycle(distA, breaksA[i], breaksA[i + 1]);
            QVector<double> va = extractCycle(voltA, breaksA[i], breaksA[i + 1]);
            QVector<double> db = extractCycle(distB, breaksB[i], breaksB[i + 1]);
            QVector<double> vb = extractCycle(voltB, breaksB[i], breaksB[i + 1]);
            QVector<double> dc = extractCycle(distC, breaksC[i], breaksC[i + 1]);
            QVector<double> vc = extractCycle(voltC, breaksC[i], breaksC[i + 1]);

            int maxLen = std::max({ da.size(), db.size(), dc.size() });

            extendToSize(da, maxLen);
            extendToSize(va, maxLen);
            extendToSize(db, maxLen);
            extendToSize(vb, maxLen);
            extendToSize(dc, maxLen);
            extendToSize(vc, maxLen);

            allDistA << da; allVoltA << va;
            allDistB << db; allVoltB << vb;
            allDistC << dc; allVoltC << vc;

            cycleLens << maxLen;
            qDebug() << QString("[Cycle %1] Lengths => A:%2, B:%3, C:%4, Final:%5")
                            .arg(i + 1)
                            .arg(da.size()).arg(db.size()).arg(dc.size()).arg(maxLen);

            qDebug() << "allDistA:" << allDistA << "allDistB:" << allDistB << "allDistC:" << allDistC;
        }

        int finalLen = *std::max_element(cycleLens.begin(), cycleLens.end());
        QJsonArray avgDistA, avgVoltA, avgDistB, avgVoltB, avgDistC, avgVoltC;

        for (int i = 0; i < finalLen; ++i) {
            double sum = 0;

            sum = 0; for (const auto& vec : allDistA) if (i < vec.size()) sum += vec[i];
            avgDistA.append(sum / cycleCount);

            sum = 0; for (const auto& vec : allVoltA) if (i < vec.size()) sum += vec[i];
            avgVoltA.append(sum / cycleCount);

            sum = 0; for (const auto& vec : allDistB) if (i < vec.size()) sum += vec[i];
            avgDistB.append(sum / cycleCount);

            sum = 0; for (const auto& vec : allVoltB) if (i < vec.size()) sum += vec[i];
            avgVoltB.append(sum / cycleCount);

            sum = 0; for (const auto& vec : allDistC) if (i < vec.size()) sum += vec[i];
            avgDistC.append(sum / cycleCount);

            sum = 0; for (const auto& vec : allVoltC) if (i < vec.size()) sum += vec[i];
            avgVoltC.append(sum / cycleCount);
        }

        qDebug() << "[Pattern] Final averaged length:" << finalLen;

        // Save to CSV
        const QString patternDir = QDir(PATTERN_PATH).filePath(ftpDate);
        ensureDirExists(patternDir);
        QString csvPath = QDir(patternDir).filePath(QString("%1.csv").arg(ftpTime));
        QFile file(csvPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "DistancePhaseA(km),VoltageA(mV),DistancePhaseB(km),VoltageB(mV),DistancePhaseC(km),VoltageC(mV)\n";
            for (int i = 0; i < finalLen; ++i) {
                out << avgDistA[i].toDouble() << "," << avgVoltA[i].toDouble() << ","
                    << avgDistB[i].toDouble() << "," << avgVoltB[i].toDouble() << ","
                    << avgDistC[i].toDouble() << "," << avgVoltC[i].toDouble() << "\n";
            }
            file.close();
        }

        // Send via FTP
        QString patternCsvName = QString("%1.csv").arg(ftpTime);
        ftpPattern = QString("lftp -u %1,%2 %3 -e 'set net:timeout 5; set net:reconnect-interval-base 1; set net:max-retries 1; cd %4/%5/%6/%7/%8/%9; put %10 -o %11 bye'")
                         .arg(FTP_Param_->USERNAME)
                         .arg(FTP_Param_->PASSWORD)
                         .arg(FTP_Param_->FTP_IP)
                         .arg(ftpPatternRoot)
                         .arg(ftpSubstation)
                         .arg(ftpLine)
                         .arg("Pattern")
                         .arg(ftpDate)
                         .arg(ftpTime)
                         .arg(csvPath)
                         .arg(patternCsvName);

        qDebug() << "[Pattern] ftpPattern command:" << ftpPattern;
        // emit startFtpTimer(ftpUrlFolder.toUtf8(), ftpUrlFolder2.toUtf8(), ftpPattern.toUtf8());
    }else {
        ftpUrlNew = "";
        ftpUrl = "";
    }

    // qWarning() << "rewritePattern fullPathPattern:" << fullPathPattern << " fullPicPATH:" << fullPicPATH;
    // if (fullPicPATH.isEmpty() && modeName != "Pattern") {
    //     qWarning() << "if [writeCSV] fullPicPATH is empty, skip updatePATHEmail"
    //                << "modeName =" << modeName
    //                << "fullPathPattern =" << fullPathPattern
    //                << "fullPicPATH =" << fullPicPATH;
    // } else {
        // qWarning() << "else [writeCSV] fullPicPATH is empty, skip updatePATHEmail"
        //            << "modeName =" << modeName
        //            << "fullPathPattern =" << fullPathPattern
        //            << "fullPicPATH =" << fullPicPATH;
    // }
    QString eventFtpRoot;
    if (eventMode == QStringLiteral("Surge")) {
        eventFtpRoot = ftpSurgeRoot;
    } else if (eventMode == QStringLiteral("Relay")) {
        eventFtpRoot = ftpRelayRoot;
    } else if (eventMode == QStringLiteral("Manual")) {
        eventFtpRoot = ftpManualRoot;
    } else if (eventMode == QStringLiteral("Periodic")) {
        eventFtpRoot = ftpPeriodicRoot;
    } else if (eventMode == QStringLiteral("Pattern")) {
        eventFtpRoot = ftpPatternRoot;
    }

    fullPicPATH = eventFtpRoot.isEmpty()
        ? QString()
        : buildFtpEventDir(
              eventFtpRoot,
              ftpSubstation,
              ftpLine,
              eventMode,
              ftpDate,
              ftpTime);
    // qWarning() << "dbases fullPicPATH" << fullPicPATH;
    emit updatePATHEmail(fullPathPattern, fullPicPATH);
    // qWarning() << "DEBUGmodeNameinFTPFUNCTION" << modeName << ftpUrl;
    if (!ftpUrlNew.isEmpty() && eventMode != QStringLiteral("Pattern")) {
        // Hard event contract: Event CSV FTP is deferred until the Picture
        // transaction proves both mandatory files are physically valid/stable.
        qDebug() << "[EVENT-FTP][DEFERRED] suppress early Event CSV upload until mandatory gate:"
                 << "mode=" << eventMode
                 << "localCSV=" << FullName;
    }
    //    emit uploadFile(FullName, ftpUrl, FTP_Param_->USERNAME, FTP_Param_->PASSWORD);
    emit uploadToSNMP(FullName, fileName);
    // qWarning()<<"fuhiuidfjuifhdfuiijfhdudfuihdfiuhifhfhdfyuerwgtfgewyugsfyugwefuygewyugf"<<ftpUrl<<"fileName"<<fileName;
}

void Database::rewritePattern(QJsonArray distA, QJsonArray voltA,
                              QJsonArray distB, QJsonArray voltB,
                              QJsonArray distC, QJsonArray voltC) {

    auto findCycleIndices = [](const QJsonArray& distArray) {
        QList<int> indices;
        indices << 0;
        for (int i = 1; i < distArray.size(); ++i) {
            if (distArray[i].toDouble() < distArray[i - 1].toDouble()) {
                indices << i;
            }
        }
        indices << distArray.size(); // ปิดท้าย
        return indices;
    };

    QList<int> breaksA = findCycleIndices(distA);
    QList<int> breaksB = findCycleIndices(distB);
    QList<int> breaksC = findCycleIndices(distC);

    int cycleCount = std::min({ breaksA.size()-1, breaksB.size()-1, breaksC.size()-1 });

    int maxLen = 0;
    QVector<QVector<double>> allDistA, allVoltA;
    QVector<QVector<double>> allDistB, allVoltB;
    QVector<QVector<double>> allDistC, allVoltC;

    auto extractCycle = [](const QJsonArray& arr, int start, int end) {
        QVector<double> result;
        for (int i = start; i < end; ++i)
            result.append(arr[i].toDouble());
        return result;
    };

    for (int i = 0; i < cycleCount; ++i) {
        auto da = extractCycle(distA, breaksA[i], breaksA[i+1]);
        auto va = extractCycle(voltA, breaksA[i], breaksA[i+1]);
        auto db = extractCycle(distB, breaksB[i], breaksB[i+1]);
        auto vb = extractCycle(voltB, breaksB[i], breaksB[i+1]);
        auto dc = extractCycle(distC, breaksC[i], breaksC[i+1]);
        auto vc = extractCycle(voltC, breaksC[i], breaksC[i+1]);

        maxLen = std::max({ maxLen, da.size(), db.size(), dc.size() });

        allDistA << da;
        allVoltA << va;
        allDistB << db;
        allVoltB << vb;
        allDistC << dc;
        allVoltC << vc;
    }

    auto extendToSize = [](QVector<double>& vec, int targetSize) {
        if (vec.isEmpty()) return;
        double last = vec.last();
        while (vec.size() < targetSize)
            vec.append(last);
    };

    // เติมทุก cycle ให้เท่ากับ maxLen
    for (int i = 0; i < cycleCount; ++i) {
        extendToSize(allDistA[i], maxLen);
        extendToSize(allVoltA[i], maxLen);
        extendToSize(allDistB[i], maxLen);
        extendToSize(allVoltB[i], maxLen);
        extendToSize(allDistC[i], maxLen);
        extendToSize(allVoltC[i], maxLen);
    }

    // คำนวณค่าเฉลี่ย
    QJsonArray avgDistA, avgVoltA, avgDistB, avgVoltB, avgDistC, avgVoltC;
    for (int i = 0; i < maxLen; ++i) {
        double sum = 0;

        sum = 0; for (const auto& vec : allDistA) sum += vec[i];
        avgDistA.append(sum / cycleCount);

        sum = 0; for (const auto& vec : allVoltA) sum += vec[i];
        avgVoltA.append(sum / cycleCount);

        sum = 0; for (const auto& vec : allDistB) sum += vec[i];
        avgDistB.append(sum / cycleCount);

        sum = 0; for (const auto& vec : allVoltB) sum += vec[i];
        avgVoltB.append(sum / cycleCount);

        sum = 0; for (const auto& vec : allDistC) sum += vec[i];
        avgDistC.append(sum / cycleCount);

        sum = 0; for (const auto& vec : allVoltC) sum += vec[i];
        avgVoltC.append(sum / cycleCount);
    }

    qDebug() << "[rewritePattern] Cycle count =" << cycleCount;
    qDebug() << "Average length =" << maxLen;


}

void Database::writeCSVSinglePhase(const QString &fileName, const QString &FullName, const QJsonArray &dist, const QJsonArray &volt) {
    qDebug() << "write success:";
    QFile file(FullName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning("Unable to open file for writing.");
        return;
    }

    QTextStream out(&file);

    // CSV header
    out << "Distance(km),Voltage(mV)\n";

    int maxSize = dist.size();

    // Loop through the data
    for (int i = 0; i < maxSize; ++i) {
        QString row;

        // Write distance and voltage values
        row += QString::number(dist[i].toDouble()) + "," + QString::number(volt[i].toDouble());

        out << row << "\n";
    }

    file.close();
    qDebug() << "write success:";

    QString sanitizedFileName = QFileInfo(fileName).fileName().replace("#", "");
    QString ftpUrl = QString("ftps://192.168.10.170/test_pictures/%1.csv").arg(sanitizedFileName);

    emit uploadFile(FullName, ftpUrl, FTP_Param_->USERNAME, FTP_Param_->PASSWORD);
    emit uploadToSNMP(FullName, fileName);
    qDebug() << "File uploaded to FTP:" << ftpUrl << "FileName:" << fileName;
    db.close();
}

void Database::saveCsvFileAndUpdateDb(int category_id, QString saveTimeInMysql, QString fileName, QString fullPath) {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        db.open();
    }

    QSqlQuery query;
    query.prepare("INSERT INTO event_records (category_id, event_datetime, filename, full_path) VALUES (?, ?, ?, ?)");
    query.addBindValue(category_id);
    query.addBindValue(saveTimeInMysql);
    query.addBindValue(fileName);
    query.addBindValue(fullPath);

    if (!query.exec()) {
        qDebug() << "Failed to insert into database:" << query.lastError().text();
    } else {
        qDebug() << "Database entry saved successfully!";
        db.commit();
    }
    db.close();
}

void Database::deleteCsvFileAndFolder(QString fileName, QString category, QString date)
{
    qDebug() << "deleteCsvFileAndFolder DEBUG" << fileName << category << date;

    const QString cleanCategory = canonicalEventCategory(category);
    if (cleanCategory.isEmpty()) {
        qWarning() << "[deleteCsvFileAndFolder] invalid category:" << category;
        return;
    }

    QString datefile;
    QString timefile;
    if (!parseEventDateTimeParts(date, datefile, timefile)) {
        qWarning() << "[deleteCsvFileAndFolder] invalid event datetime:" << date;
        return;
    }

    const QString safeName = safeEventFileName(fileName);
    const QString filePath = safeJoinEventPath(EVENT_PATH, cleanCategory, datefile, timefile, safeName);
    if (filePath.isEmpty()) {
        qWarning() << "[deleteCsvFileAndFolder] unsafe file path";
        return;
    }

    const QString marginPath = filePath + QStringLiteral("_Margin");

    auto removeFileIfExists = [](const QString &path) {
        QFile file(path);
        if (!file.exists()) {
            qDebug() << "File does not exist:" << path;
            return;
        }

        if (file.remove()) {
            qDebug() << "File deleted successfully:" << path;
        } else {
            qWarning() << "Failed to delete file:" << path << file.errorString();
        }
    };

    removeFileIfExists(filePath);
    removeFileIfExists(marginPath);

    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);
    query.prepare("DELETE FROM event_records WHERE filename = :filename AND event_datetime = :event_datetime");
    query.bindValue(":filename", safeName);
    query.bindValue(":event_datetime", date);

    if (!query.exec()) {
        qDebug() << "Query failed:" << query.lastError().text();
        return;
    }

    const QString dirPath = safeJoinEventPath(EVENT_PATH, cleanCategory, datefile, timefile);
    if (!dirPath.isEmpty()) {
        QDir dir(dirPath);
        if (dir.exists()) {
            const QFileInfoList remain = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
            if (remain.isEmpty()) {
                QDir parent(QFileInfo(dirPath).absolutePath());
                if (parent.rmdir(QFileInfo(dirPath).fileName())) {
                    qDebug() << "Directory deleted successfully:" << dirPath;
                } else {
                    qWarning() << "Failed to delete directory:" << dirPath;
                }
            } else {
                qDebug() << "Directory not empty, keep:" << dirPath;
            }
        }
    }

    getNewdatafromDB(cleanCategory);
    if (cleanCategory == QStringLiteral("Pattern")) {
        deleteSelectPattern();
    }
    db.close();
}

void Database::getNewdatafromDB(QString category) {
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query(db);
    QString queryStr = QString("SELECT e.event_datetime, e.filename, c.name AS category_name  FROM event_records e  JOIN categories c ON e.category_id = c.id  WHERE c.name = '%1' ORDER BY e.event_datetime ASC").arg(category);

    if (!query.exec(queryStr)) {
        qDebug() << "Query failed:" << query.lastError().text();
        return;
    }
    db.close();
    patternDataDb(query);
    //    qDebug() << "Searching for filename:" << date;
}

// void Database::getCsvFile(QString fileName, QString category, QString date){

//    QString datefile = date.left(10);  // "2025-02-18"
//    QString timefile = date.mid(11, 8);  // "03:30:17"
//    timefile.replace(":", "-");  // Replace ":" with "-" to match the folder format

//    // Construct the full file path based on the directory structure
//    QString filePath = QString("/home/pi/event_records/%1/%2/%3/%4").arg(category).arg(datefile).arg(timefile).arg(fileName);

//    QFile file(filePath);
//    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
//            qWarning("Unable to open file for reading.");
//            return;
//        }

//        QTextStream in(&file);

//        // ข้ามบรรทัดแรก (Header)
//        if (!in.atEnd()) {
//            in.readLine();
//        }

//        while (!in.atEnd()) {
//            QString line = in.readLine();
//            QStringList values = line.split(",");

//            if (values.size() == 6) {
//                distA.append(values[0].toDouble());
//                voltA.append(values[1].toDouble());

//                distB.append(values[2].toDouble());
//                voltB.append(values[3].toDouble());

//                distC.append(values[4].toDouble());
//                voltC.append(values[5].toDouble());
//            }
//        }

//        file.close();

////        QJsonObject mainObject;
////        mainObject.insert("objectName", "dataPlotingA");
////        mainObject.insert("distance", distA);
////        mainObject.insert("voltage", voltA);
////        qDebug() << "Starting DebugA4.";
////        QJsonDocument jsonDoc(mainObject);
////        QString raw_data = jsonDoc.toJson(QJsonDocument::Compact);
////    //    qDebug() << "Generated JSON for smoothed curve:" << raw_data;

////        QString rawdataArrayA = std::move(raw_data);
////    //    qDebug() << "Generated JSON for smoothed curveA:" << rawdataArrayA;
////    //    qDebug() << "distanceA:" << dist << "voltage:" << volt << "modeName" <<modeName << "FileTimeStamp" << FileTimeStamp;
////        // Emit the signal for the curve
////        emit sendToMonitor(rawdataArrayA);
////        qDebug() << "Distance A:" << distA;
////        qDebug() << "Voltage A:" << voltA;
////        qDebug() << "Distance B:" << distB;
////        qDebug() << "Voltage B:" << voltB;
////        qDebug() << "Distance C:" << distC;
////        qDebug() << "Voltage C:" << voltC;
//}

void Database::updateFTPParam(FTP_Param *ftp) {
    *FTP_Param_ = *ftp;
    FTP_Param_->printinfo();
}

void Database::uploadCSVTower()
{
    qDebug() << "uploadCSVTower";

    const QString srcPath = QStringLiteral("/var/www/html/uploads/tower.csv");
    const QString dstDir = QDir::cleanPath(TOWER_NO);
    const QString dstPath = QDir(dstDir).filePath(QStringLiteral("tower.csv"));

    QFileInfo srcInfo(srcPath);
    if (!srcInfo.exists() || !srcInfo.isFile()) {
        qDebug() << "File does not exist:" << srcPath;
        return;
    }

    if (!ensureDirExists(dstDir)) {
        qWarning() << "[uploadCSVTower] cannot create destination directory:" << dstDir;
        return;
    }

    QFile::remove(dstPath);
    if (!QFile::copy(srcPath, dstPath)) {
        qWarning() << "[uploadCSVTower] copy failed:" << srcPath << "->" << dstPath;
        return;
    }

    qDebug() << "[uploadCSVTower] copied:" << srcPath << "->" << dstPath;

    if (QFile::remove(srcPath)) {
        emit sendToWeb();
        qDebug() << "Upload source deleted successfully.";
    } else {
        qWarning() << "Failed to delete upload source:" << srcPath;
    }

    insertCSVTower();
}

void Database::insertCSVTower() {
    qDebug() << "insertCSVTower ... !";
    QFile file(TOWER_NO + "tower.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open CSV file: " << file.errorString();
        return;
    }

    QTextStream in(&file);
    QString headers = in.readLine();  // Read and skip the first line (headers)

    int rowCount = 0;

    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query;
    query.prepare("TRUNCATE TABLE TransmissionData");

    if (query.exec()) {
        qDebug() << "Table truncated: " << "TransmissionData";
    } else {
        qDebug() << "Failed to truncate table: " << query.lastError().text();
    }

    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList fields = line.split(",");

        // Ensure we have exactly 4 fields
        //        if (fields.size() != 4) {
        //            qDebug() << "Invalid CSV line, skipping: " << line;
        //            continue;
        //        }

        // Prepare the SQL query (skip the auto-increment id)
        QSqlQuery query;
        query.prepare(
            "INSERT INTO TransmissionData (TowerNo, Distance, TransmissionLine, FullDistance) "
            "VALUES (:TowerNo, :Distance, :TransmissionLine, :FullDistance)");

        // Bind the values from the CSV
        query.bindValue(":TowerNo", fields[1].trimmed());
        query.bindValue(":Distance", fields[2].trimmed());
        query.bindValue(":TransmissionLine", fields[3].trimmed());
        query.bindValue(":FullDistance", fields[4].trimmed());

        // Execute the query
        if (!query.exec()) {
            qDebug() << "Insert failed: " << query.lastError().text();
        } else {
            qDebug() << "Inserted row: " << fields.join(", ");
            rowCount++;
        }
    }

    db.close();
    file.close();
    qDebug() << "CSV upload complete! Total rows inserted: " << rowCount;
}

void Database::selectDataCSVTower() {
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }

    QSqlQuery query;
    query.prepare("SELECT * FROM TransmissionData;");
    if (!query.exec()) {
        qDebug() << "Error executing SELECT query: " << query.lastError().text();
        return;
    }
    //    QString TransmissionLine;
    //    float FullDistance;
    //    QList<QList<float>> Distance;
    //    QList<QList<QString>> TowerNo;
    // Process the results
    while (query.next()) {
        if (query.value("id") == "") {
            return;
        }
        int id = query.value("id").toInt();                                     // Retrieve `id` (Auto-incremented field)
        QString towerNo = query.value("TowerNo").toString();                    // Retrieve `TowerNo`
        QString distance = query.value("Distance").toString();                  // Retrieve `Distance`
        QString transmissionLine = query.value("TransmissionLine").toString();  // Retrieve `TransmissionLine`
        QString fullDistance = query.value("FullDistance").toString();          // Retrieve `FullDistance`

        // Print the results
        if (id == 1) {
            TransmissionLine = transmissionLine;
            FullDistance = fullDistance.toFloat();
        }
        TowerNo.append(towerNo);
        Distance.append(distance.toFloat());
        //        qDebug() << "ID:" << id
        //                 << "TowerNo:" << towerNo
        //                 << "Distance:" << distance
        //                 << "TransmissionLine:" << transmissionLine
        //                 << "FullDistance:" << fullDistance;
    }

    query.prepare("UPDATE DisplaySetting SET FullDistance = :fullDistance WHERE number = 1;");
    query.bindValue(":fullDistance", FullDistance);
    if (!query.exec()) {
        qDebug() << "Error executing UPDATE query: " << query.lastError().text();
        return;
    } else {
        qDebug() << "FullDistance updated successfully to" << FullDistance;
    }

    db.close();
}

void Database::updatePathNotMount(QString tower, QString event) {
    QString EVENT_PATH = event;
    QString TOWER_NO = tower;
}

namespace {

QStringList eventBasePaths()
{
    // สำคัญ: ห้ามใส่ Pattern ใน list นี้
    // เพราะ list นี้ถูกใช้เป็นฐาน path สำหรับการลบไฟล์/โฟลเดอร์เก่า
    return {
        "/mnt/sdcard/event_records/Pic",
        "/mnt/sdcard/event_records/Manual",
        "/mnt/sdcard/event_records/Relay",
        "/mnt/sdcard/event_records/Surge",
        "/mnt/sdcard/event_records/Periodic"
    };
}

bool isInsideAllowedEventPath(const QString &path)
{
    const QString cleanPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());

    for (const QString &base : eventBasePaths()) {
        const QString cleanBase = QDir::cleanPath(QFileInfo(base).absoluteFilePath());

        if (cleanPath == cleanBase) {
            return true;
        }

        if (cleanPath.startsWith(cleanBase + "/")) {
            return true;
        }
    }

    return false;
}

bool isBaseEventPath(const QString &path)
{
    const QString cleanPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());

    for (const QString &base : eventBasePaths()) {
        const QString cleanBase = QDir::cleanPath(QFileInfo(base).absoluteFilePath());

        if (cleanPath == cleanBase) {
            return true;
        }
    }

    return false;
}

void removeEmptyParentFoldersUntilBase(const QString &fileOrFolderPath,
                                       int &deletedDirs,
                                       int &failedCount)
{
    QFileInfo info(fileOrFolderPath);
    QString dirPath = info.isDir() ? info.absoluteFilePath() : info.absolutePath();

    while (!dirPath.isEmpty()) {
        if (!isInsideAllowedEventPath(dirPath)) {
            break;
        }

        if (isBaseEventPath(dirPath)) {
            break;
        }

        QDir dir(dirPath);

        if (!dir.exists()) {
            dirPath = QFileInfo(dirPath).absolutePath();
            continue;
        }

        const QFileInfoList remain = dir.entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System
            );

        if (!remain.isEmpty()) {
            break;
        }

        QDir parent(QFileInfo(dirPath).absolutePath());
        const QString folderName = QFileInfo(dirPath).fileName();

        if (parent.rmdir(folderName)) {
            deletedDirs++;
            qDebug() << "[deleteOldFilesAndRecords] deleted empty folder:" << dirPath;
        } else {
            failedCount++;
            qWarning() << "[deleteOldFilesAndRecords] failed to delete empty folder:" << dirPath;
            break;
        }

        dirPath = parent.absolutePath();
    }
}

void cleanupOldPhysicalPathRecursive(const QString &path,
                                     const QDateTime &expireTime,
                                     int &deletedFiles,
                                     int &deletedDirs,
                                     int &failedCount)
{
    QFileInfo info(path);

    if (!info.exists()) {
        return;
    }

    if (!isInsideAllowedEventPath(info.absoluteFilePath())) {
        qWarning() << "[deleteOldFilesAndRecords] skip unsafe path:" << info.absoluteFilePath();
        return;
    }

    if (isBaseEventPath(info.absoluteFilePath())) {
        QDir baseDir(info.absoluteFilePath());

        const QFileInfoList entries = baseDir.entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
            QDir::DirsFirst
            );

        for (const QFileInfo &entry : entries) {
            cleanupOldPhysicalPathRecursive(entry.absoluteFilePath(),
                                            expireTime,
                                            deletedFiles,
                                            deletedDirs,
                                            failedCount);
        }

        return;
    }

    // symlink ให้ลบเฉพาะ link ไม่ตามไปลบปลายทาง
    if (info.isSymLink() || info.isFile()) {
        const QDateTime lastModified = info.lastModified();

        if (lastModified.isValid() && lastModified < expireTime) {
            if (QFile::remove(info.absoluteFilePath())) {
                deletedFiles++;
                qDebug() << "[deleteOldFilesAndRecords] deleted file:"
                         << info.absoluteFilePath()
                         << "lastModified=" << lastModified.toString(Qt::ISODate);

                // ลบ parent folder ที่ว่าง เช่น HH-MM-SS และ YYYY-MM-DD
                removeEmptyParentFoldersUntilBase(info.absoluteFilePath(),
                                                  deletedDirs,
                                                  failedCount);
            } else {
                failedCount++;
                qWarning() << "[deleteOldFilesAndRecords] failed to delete file:"
                           << info.absoluteFilePath();
            }
        }

        return;
    }

    if (!info.isDir()) {
        return;
    }

    QDir dir(info.absoluteFilePath());

    const QFileInfoList entries = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
        QDir::DirsFirst
        );

    for (const QFileInfo &entry : entries) {
        cleanupOldPhysicalPathRecursive(entry.absoluteFilePath(),
                                        expireTime,
                                        deletedFiles,
                                        deletedDirs,
                                        failedCount);
    }

    QFileInfo refreshed(info.absoluteFilePath());

    if (!refreshed.exists()) {
        return;
    }

    QDir refreshedDir(refreshed.absoluteFilePath());

    const QFileInfoList remain = refreshedDir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System
        );

    const QDateTime folderModified = refreshed.lastModified();

    if (remain.isEmpty() &&
        !isBaseEventPath(refreshed.absoluteFilePath())) {

        QDir parent(refreshed.absolutePath());

        if (parent.rmdir(refreshed.fileName())) {
            deletedDirs++;
            qDebug() << "[deleteOldFilesAndRecords] deleted empty folder:"
                     << refreshed.absoluteFilePath();
        } else {
            failedCount++;
            qWarning() << "[deleteOldFilesAndRecords] failed to delete empty folder:"
                       << refreshed.absoluteFilePath();
        }
    }
}

}

namespace {

const QString EVENT_ROOT_PATH = "/mnt/sdcard/event_records";

QStringList eventCleanupBasePaths()
{
    // Emergency cleanup ก็ต้องไม่แตะ Pattern เช่นกัน
    return {
        "/mnt/sdcard/event_records/Pic",
        "/mnt/sdcard/event_records/Manual",
        "/mnt/sdcard/event_records/Relay",
        "/mnt/sdcard/event_records/Surge",
        "/mnt/sdcard/event_records/Periodic"
    };
}

bool isAllowedEventCleanupPath(const QString &path)
{
    const QString cleanPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());

    for (const QString &base : eventCleanupBasePaths()) {
        const QString cleanBase = QDir::cleanPath(QFileInfo(base).absoluteFilePath());

        if (cleanPath == cleanBase || cleanPath.startsWith(cleanBase + "/")) {
            return true;
        }
    }

    return false;
}

bool removeDirectoryContentsOnly(const QString &dirPath,
                                 int &deletedFiles,
                                 int &deletedDirs,
                                 int &failedCount)
{
    const QString cleanDirPath = QDir::cleanPath(QFileInfo(dirPath).absoluteFilePath());

    if (!isAllowedEventCleanupPath(cleanDirPath)) {
        qWarning() << "[EmergencyCleanup] unsafe base path, skip:" << cleanDirPath;
        failedCount++;
        return false;
    }

    QDir dir(cleanDirPath);

    if (!dir.exists()) {
        qWarning() << "[EmergencyCleanup] base path not found, create:" << cleanDirPath;

        if (!QDir().mkpath(cleanDirPath)) {
            qWarning() << "[EmergencyCleanup] failed to create base path:" << cleanDirPath;
            failedCount++;
            return false;
        }

        return true;
    }

    const QFileInfoList entries = dir.entryInfoList(
        QDir::AllEntries |
            QDir::NoDotAndDotDot |
            QDir::Hidden |
            QDir::System,
        QDir::DirsFirst
        );

    bool ok = true;

    for (const QFileInfo &entry : entries) {
        const QString entryPath = QDir::cleanPath(entry.absoluteFilePath());

        if (!isAllowedEventCleanupPath(entryPath)) {
            qWarning() << "[EmergencyCleanup] unsafe entry, skip:" << entryPath;
            failedCount++;
            ok = false;
            continue;
        }

        if (entry.isDir() && !entry.isSymLink()) {
            QDir subDir(entryPath);

            if (subDir.removeRecursively()) {
                deletedDirs++;
                qDebug() << "[EmergencyCleanup] deleted folder:" << entryPath;
            } else {
                failedCount++;
                ok = false;
                qWarning() << "[EmergencyCleanup] failed to delete folder:" << entryPath;
            }
        } else {
            if (QFile::remove(entryPath)) {
                deletedFiles++;
                qDebug() << "[EmergencyCleanup] deleted file:" << entryPath;
            } else {
                failedCount++;
                ok = false;
                qWarning() << "[EmergencyCleanup] failed to delete file:" << entryPath;
            }
        }
    }

    // คง base folder ไว้ เช่น Pic / Manual / Relay
    QDir().mkpath(cleanDirPath);

    return ok;
}

}

namespace {

static const int kEventRetentionLimitPerCategory = 100;

struct RetentionBaseSpec
{
    RetentionBaseSpec(const QString &categoryValue,
                      const QString &basePathValue,
                      bool hasDatabaseRecordsValue)
        : category(categoryValue),
          basePath(basePathValue),
          hasDatabaseRecords(hasDatabaseRecordsValue)
    {
    }

    QString category;
    QString basePath;
    bool hasDatabaseRecords;
};

struct RetentionEventSet
{
    QString category;
    QString basePath;
    QString path;
    QDateTime eventDateTime;
    QDateTime lastModified;
};

QList<RetentionBaseSpec> retentionBaseSpecs()
{
    /*
     * Pattern is intentionally NOT present here.
     * Requirement: Pattern is unlimited and must never be removed by retention.
     *
     * One "set" is one event time-folder:
     *   /mnt/sdcard/event_records/<Category>/yyyy-MM-dd/HH-mm-ss/
     * or for pictures:
     *   /mnt/sdcard/event_records/Pic/yyyy-MM-dd/HH-mm-ss/
     */
    return {
        { QStringLiteral("Pic"),      QStringLiteral("/mnt/sdcard/event_records/Pic"),      false },
        { QStringLiteral("Manual"),   QStringLiteral("/mnt/sdcard/event_records/Manual"),   true  },
        { QStringLiteral("Relay"),    QStringLiteral("/mnt/sdcard/event_records/Relay"),    true  },
        { QStringLiteral("Surge"),    QStringLiteral("/mnt/sdcard/event_records/Surge"),    true  },
        { QStringLiteral("Periodic"), QStringLiteral("/mnt/sdcard/event_records/Periodic"), true  }
    };
}

QDateTime retentionDateTimeFromParts(const QString &datePart,
                                     const QString &timePart)
{
    const QDate date = QDate::fromString(datePart.trimmed(), QStringLiteral("yyyy-MM-dd"));

    QTime time = QTime::fromString(timePart.trimmed(), QStringLiteral("HH-mm-ss"));
    if (!time.isValid()) {
        time = QTime::fromString(timePart.trimmed(), QStringLiteral("HH:mm:ss"));
    }

    if (!date.isValid() || !time.isValid()) {
        return QDateTime();
    }

    return QDateTime(date, time);
}

qint64 countFilesRecursive(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists()) {
        return 0;
    }

    if (info.isFile() || info.isSymLink()) {
        return 1;
    }

    if (!info.isDir()) {
        return 0;
    }

    qint64 count = 0;
    QDir dir(info.absoluteFilePath());
    const QFileInfoList entries = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
        QDir::DirsFirst | QDir::Name
    );

    for (const QFileInfo &entry : entries) {
        count += countFilesRecursive(entry.absoluteFilePath());
    }

    return count;
}

QList<RetentionEventSet> collectRetentionEventSets(const RetentionBaseSpec &spec)
{
    QList<RetentionEventSet> result;

    const QString cleanBase = QDir::cleanPath(QFileInfo(spec.basePath).absoluteFilePath());
    QDir baseDir(cleanBase);

    if (!baseDir.exists()) {
        qWarning() << "[Retention100] base path not found; create and continue:"
                   << cleanBase;
        QDir().mkpath(cleanBase);
        return result;
    }

    const QFileInfoList baseEntries = baseDir.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
        QDir::Name
    );

    for (const QFileInfo &dateEntry : baseEntries) {
        if (dateEntry.isSymLink() || dateEntry.isFile()) {
            // Legacy flat file: count one physical file as one set.
            RetentionEventSet set;
            set.category = spec.category;
            set.basePath = cleanBase;
            set.path = dateEntry.absoluteFilePath();
            set.lastModified = dateEntry.lastModified();
            set.eventDateTime = set.lastModified;
            result.append(set);
            continue;
        }

        if (!dateEntry.isDir()) {
            continue;
        }

        const QString datePart = dateEntry.fileName();
        QDir dateDir(dateEntry.absoluteFilePath());
        const QFileInfoList timeEntries = dateDir.entryInfoList(
            QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
            QDir::Name
        );

        if (timeEntries.isEmpty()) {
            // Empty date folders are not event sets; clean them later if possible.
            continue;
        }

        for (const QFileInfo &timeEntry : timeEntries) {
            RetentionEventSet set;
            set.category = spec.category;
            set.basePath = cleanBase;
            set.path = timeEntry.absoluteFilePath();
            set.lastModified = timeEntry.lastModified();

            if (timeEntry.isDir() && !timeEntry.isSymLink()) {
                set.eventDateTime = retentionDateTimeFromParts(datePart, timeEntry.fileName());
            }

            if (!set.eventDateTime.isValid()) {
                // Legacy/unexpected layout: lastModified is only a fallback for ordering.
                set.eventDateTime = set.lastModified;
            }

            result.append(set);
        }
    }

    std::sort(result.begin(), result.end(), [](const RetentionEventSet &a,
                                                const RetentionEventSet &b) {
        if (a.eventDateTime != b.eventDateTime) {
            return a.eventDateTime > b.eventDateTime; // newest first
        }
        if (a.lastModified != b.lastModified) {
            return a.lastModified > b.lastModified;
        }
        return a.path > b.path;
    });

    return result;
}

bool removeRetentionPhysicalSet(const RetentionEventSet &set,
                                qint64 &deletedFiles,
                                int &deletedDirs,
                                int &failedCount)
{
    const QString cleanPath = QDir::cleanPath(QFileInfo(set.path).absoluteFilePath());
    const QString cleanBase = QDir::cleanPath(QFileInfo(set.basePath).absoluteFilePath());

    if (cleanPath.isEmpty() || cleanPath == cleanBase ||
        !cleanPath.startsWith(cleanBase + QStringLiteral("/"))) {
        failedCount++;
        qWarning() << "[Retention100][BLOCK] unsafe deletion path:"
                   << cleanPath << "base=" << cleanBase;
        return false;
    }

    QFileInfo info(cleanPath);
    if (!info.exists()) {
        qDebug() << "[Retention100] physical set already missing:" << cleanPath;
        return true;
    }

    if (info.isSymLink() || info.isFile()) {
        if (QFile::remove(cleanPath)) {
            deletedFiles++;
            removeEmptyParentFoldersUntilBase(cleanPath, deletedDirs, failedCount);
            return true;
        }

        failedCount++;
        qWarning() << "[Retention100] failed to delete file set:" << cleanPath;
        return false;
    }

    if (!info.isDir()) {
        failedCount++;
        qWarning() << "[Retention100] unsupported physical set type:" << cleanPath;
        return false;
    }

    const qint64 filesInside = countFilesRecursive(cleanPath);
    QDir setDir(cleanPath);

    if (!setDir.removeRecursively()) {
        failedCount++;
        qWarning() << "[Retention100] failed to delete event folder:" << cleanPath;
        return false;
    }

    deletedFiles += filesInside;
    deletedDirs++;
    removeEmptyParentFoldersUntilBase(QFileInfo(cleanPath).absolutePath(),
                                      deletedDirs,
                                      failedCount);
    return true;
}

int deleteDatabaseRecordsInsideSet(QSqlDatabase &db,
                                   const QString &category,
                                   const QString &setPath,
                                   int &failedCount)
{
    if (category.compare(QStringLiteral("Pic"), Qt::CaseInsensitive) == 0) {
        return 0;
    }

    const QString cleanSetPath = QDir::cleanPath(QFileInfo(setPath).absoluteFilePath());

    struct DbRow {
        int categoryId = 0;
        QString eventDateTime;
        QString fileName;
        QString fullPath;
    };

    QList<DbRow> rowsToDelete;

    QSqlQuery selectQuery(db);
    selectQuery.prepare(
        "SELECT e.category_id, e.event_datetime, e.filename, e.full_path "
        "FROM event_records e "
        "JOIN categories c ON c.id = e.category_id "
        "WHERE LOWER(TRIM(c.name)) = LOWER(:category)"
    );
    selectQuery.bindValue(QStringLiteral(":category"), category);

    if (!selectQuery.exec()) {
        failedCount++;
        qWarning() << "[Retention100][DB] failed to read category records:"
                   << category << selectQuery.lastError().text();
        return 0;
    }

    while (selectQuery.next()) {
        const QString rowPath = QDir::cleanPath(
            QFileInfo(selectQuery.value(QStringLiteral("full_path")).toString()).absoluteFilePath()
        );

        if (rowPath == cleanSetPath || rowPath.startsWith(cleanSetPath + QStringLiteral("/"))) {
            DbRow row;
            row.categoryId = selectQuery.value(QStringLiteral("category_id")).toInt();
            row.eventDateTime = selectQuery.value(QStringLiteral("event_datetime")).toString();
            row.fileName = selectQuery.value(QStringLiteral("filename")).toString();
            row.fullPath = selectQuery.value(QStringLiteral("full_path")).toString();
            rowsToDelete.append(row);
        }
    }

    int deletedRows = 0;

    for (const DbRow &row : rowsToDelete) {
        QSqlQuery deleteQuery(db);
        deleteQuery.prepare(
            "DELETE FROM event_records "
            "WHERE category_id = :categoryId "
            "  AND event_datetime = :eventDateTime "
            "  AND filename = :fileName "
            "  AND full_path = :fullPath "
            "LIMIT 1"
        );
        deleteQuery.bindValue(QStringLiteral(":categoryId"), row.categoryId);
        deleteQuery.bindValue(QStringLiteral(":eventDateTime"), row.eventDateTime);
        deleteQuery.bindValue(QStringLiteral(":fileName"), row.fileName);
        deleteQuery.bindValue(QStringLiteral(":fullPath"), row.fullPath);

        if (deleteQuery.exec()) {
            deletedRows += deleteQuery.numRowsAffected();
        } else {
            failedCount++;
            qWarning() << "[Retention100][DB] failed to delete row:"
                       << category << row.eventDateTime << row.fileName
                       << deleteQuery.lastError().text();
        }
    }

    return deletedRows;
}

int cleanupMissingDatabaseRecordsForCategory(QSqlDatabase &db,
                                             const RetentionBaseSpec &spec,
                                             int &failedCount)
{
    if (!spec.hasDatabaseRecords) {
        return 0;
    }

    const QString cleanBase = QDir::cleanPath(QFileInfo(spec.basePath).absoluteFilePath());

    struct MissingRow {
        int categoryId = 0;
        QString eventDateTime;
        QString fileName;
        QString fullPath;
    };

    QList<MissingRow> missingRows;

    QSqlQuery selectQuery(db);
    selectQuery.prepare(
        "SELECT e.category_id, e.event_datetime, e.filename, e.full_path "
        "FROM event_records e "
        "JOIN categories c ON c.id = e.category_id "
        "WHERE LOWER(TRIM(c.name)) = LOWER(:category)"
    );
    selectQuery.bindValue(QStringLiteral(":category"), spec.category);

    if (!selectQuery.exec()) {
        failedCount++;
        qWarning() << "[Retention100][DB-RECONCILE] failed to read records:"
                   << spec.category << selectQuery.lastError().text();
        return 0;
    }

    while (selectQuery.next()) {
        const QString originalPath = selectQuery.value(QStringLiteral("full_path")).toString().trimmed();
        if (originalPath.isEmpty()) {
            // Empty path is invalid but do not delete automatically without a safe physical base.
            qWarning() << "[Retention100][DB-RECONCILE] empty full_path, keep for manual review:"
                       << spec.category
                       << selectQuery.value(QStringLiteral("filename")).toString();
            continue;
        }

        const QString cleanPath = QDir::cleanPath(QFileInfo(originalPath).absoluteFilePath());

        // Never auto-delete a DB row whose path is outside the expected category base.
        if (cleanPath != cleanBase && !cleanPath.startsWith(cleanBase + QStringLiteral("/"))) {
            qWarning() << "[Retention100][DB-RECONCILE] path outside category base, keep:"
                       << spec.category << cleanPath;
            continue;
        }

        QFileInfo info(cleanPath);
        if (info.exists()) {
            continue;
        }

        MissingRow row;
        row.categoryId = selectQuery.value(QStringLiteral("category_id")).toInt();
        row.eventDateTime = selectQuery.value(QStringLiteral("event_datetime")).toString();
        row.fileName = selectQuery.value(QStringLiteral("filename")).toString();
        row.fullPath = originalPath;
        missingRows.append(row);
    }

    int deletedRows = 0;
    for (const MissingRow &row : missingRows) {
        QSqlQuery deleteQuery(db);
        deleteQuery.prepare(
            "DELETE FROM event_records "
            "WHERE category_id = :categoryId "
            "  AND event_datetime = :eventDateTime "
            "  AND filename = :fileName "
            "  AND full_path = :fullPath "
            "LIMIT 1"
        );
        deleteQuery.bindValue(QStringLiteral(":categoryId"), row.categoryId);
        deleteQuery.bindValue(QStringLiteral(":eventDateTime"), row.eventDateTime);
        deleteQuery.bindValue(QStringLiteral(":fileName"), row.fileName);
        deleteQuery.bindValue(QStringLiteral(":fullPath"), row.fullPath);

        if (deleteQuery.exec()) {
            const int affected = deleteQuery.numRowsAffected();
            deletedRows += affected;
            if (affected > 0) {
                qDebug() << "[Retention100][DB-RECONCILE] removed stale row:"
                         << spec.category << row.eventDateTime << row.fileName;
            }
        } else {
            failedCount++;
            qWarning() << "[Retention100][DB-RECONCILE] delete failed:"
                       << spec.category << row.eventDateTime << row.fileName
                       << deleteQuery.lastError().text();
        }
    }

    return deletedRows;
}

void removeEmptyDateFoldersForRetention(const QString &basePath,
                                        int &deletedDirs,
                                        int &failedCount)
{
    QDir baseDir(basePath);
    if (!baseDir.exists()) {
        return;
    }

    const QFileInfoList dateDirs = baseDir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
        QDir::Name
    );

    for (const QFileInfo &dateInfo : dateDirs) {
        QDir dateDir(dateInfo.absoluteFilePath());

        // First remove empty HH-mm-ss folders that are not real event sets.
        const QFileInfoList timeDirs = dateDir.entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
            QDir::Name
        );

        for (const QFileInfo &timeInfo : timeDirs) {
            QDir timeDir(timeInfo.absoluteFilePath());
            const QFileInfoList timeRemain = timeDir.entryInfoList(
                QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System
            );

            if (!timeRemain.isEmpty()) {
                continue;
            }

            if (dateDir.rmdir(timeInfo.fileName())) {
                deletedDirs++;
                qDebug() << "[Retention100] deleted empty event folder:"
                         << timeInfo.absoluteFilePath();
            } else {
                failedCount++;
                qWarning() << "[Retention100] failed to delete empty event folder:"
                           << timeInfo.absoluteFilePath();
            }
        }

        const QFileInfoList remain = dateDir.entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System
        );

        if (!remain.isEmpty()) {
            continue;
        }

        if (baseDir.rmdir(dateInfo.fileName())) {
            deletedDirs++;
            qDebug() << "[Retention100] deleted empty date folder:"
                     << dateInfo.absoluteFilePath();
        } else {
            failedCount++;
            qWarning() << "[Retention100] failed to delete empty date folder:"
                       << dateInfo.absoluteFilePath();
        }
    }
}

} // namespace

void Database::checkEventStorageAndEmergencyCleanup()
{
    /*
     * Requirement change:
     * Emergency storage handling must NEVER wipe all non-Pattern events.
     * The only automatic destructive policy is the 100-set-per-category limit.
     */
    qDebug() << "[EmergencyCleanup] check event storage (non-destructive policy)";

    if (!QDir(EVENT_ROOT_PATH).exists()) {
        qWarning() << "[EmergencyCleanup] event root path not found, skip:"
                   << EVENT_ROOT_PATH;
        return;
    }

    QStorageInfo storage(EVENT_ROOT_PATH);
    storage.refresh();

    if (!storage.isValid() || !storage.isReady() || storage.bytesTotal() <= 0) {
        qWarning() << "[EmergencyCleanup] storage not valid/ready:" << EVENT_ROOT_PATH;
        return;
    }

    const qint64 totalBytes = storage.bytesTotal();
    const qint64 availableBytes = storage.bytesAvailable();
    const qint64 usedBytes = totalBytes - availableBytes;
    const double usedPercent = (static_cast<double>(usedBytes) * 100.0) /
                               static_cast<double>(totalBytes);

    qDebug() << "[EmergencyCleanup] usage="
             << QString::number(usedPercent, 'f', 2) + QStringLiteral("%")
             << "availableMB=" << availableBytes / 1024 / 1024;

    if (usedPercent < 70.0) {
        return;
    }

    qWarning() << "[EmergencyCleanup] storage >= 70%;"
               << "full wipe is disabled by retention policy."
               << "The daily 100-set retention job is the only automatic deletion.";

    /*
     * loopGetInfo() runs the retention job immediately before this storage check
     * on each date change, so do not invoke it a second time here.  Even when
     * storage remains high, never delete below the configured 100-set limit.
     */
}

void Database::deleteOldFilesAndRecords()
{
    /*
     * Compatibility name retained because PLCServer already calls this slot at
     * startup and once per day. The policy is now COUNT BASED, not age based:
     *
     *   Pic      <= 100 event sets
     *   Manual   <= 100 event sets
     *   Relay    <= 100 event sets
     *   Surge    <= 100 event sets
     *   Periodic <= 100 event sets
     *
     * Pattern is excluded completely and is unlimited.
     * Only the oldest excess sets are deleted.
     */
    qDebug() << "[Retention100] daily retention check start"
             << "limitPerCategory=" << kEventRetentionLimitPerCategory;

    /*
     * Mount safety: never turn a missing /mnt/sdcard mount into a cleanup of
     * directories on the root filesystem, and never reconcile DB rows while
     * the event storage is unavailable.
     */
    if (!QDir(EVENT_ROOT_PATH).exists()) {
        qWarning() << "[Retention100][BLOCK] event root does not exist; skip retention:"
                   << EVENT_ROOT_PATH;
        return;
    }

    QStorageInfo retentionStorage(EVENT_ROOT_PATH);
    retentionStorage.refresh();

    const QString storageRoot = QDir::cleanPath(retentionStorage.rootPath());
    if (!retentionStorage.isValid() || !retentionStorage.isReady() ||
        retentionStorage.bytesTotal() <= 0 ||
        storageRoot == QStringLiteral("/") ||
        (storageRoot != QStringLiteral("/mnt/sdcard") &&
         !storageRoot.startsWith(QStringLiteral("/mnt/sdcard/")))) {

        qWarning() << "[Retention100][BLOCK] event storage is not a verified /mnt/sdcard mount;"
                   << "rootPath=" << retentionStorage.rootPath()
                   << "device=" << retentionStorage.device()
                   << "ready=" << retentionStorage.isReady();
        return;
    }

    bool openedHere = false;
    if (!db.isOpen()) {
        if (!db.open()) {
            qWarning() << "[Retention100] failed to open database:"
                       << db.lastError().text();
            return;
        }
        openedHere = true;
    }

    qint64 deletedFiles = 0;
    int deletedDirs = 0;
    int deletedDbRows = 0;
    int failedCount = 0;
    int deletedSets = 0;

    const QList<RetentionBaseSpec> specs = retentionBaseSpecs();

    for (const RetentionBaseSpec &spec : specs) {
        QList<RetentionEventSet> sets = collectRetentionEventSets(spec);
        const int totalSets = sets.size();
        const int excess = qMax(0, totalSets - kEventRetentionLimitPerCategory);

        qDebug() << "[Retention100][CHECK]"
                 << "category=" << spec.category
                 << "base=" << spec.basePath
                 << "totalSets=" << totalSets
                 << "limit=" << kEventRetentionLimitPerCategory
                 << "excess=" << excess;

        if (excess <= 0) {
            removeEmptyDateFoldersForRetention(spec.basePath, deletedDirs, failedCount);
            deletedDbRows += cleanupMissingDatabaseRecordsForCategory(db, spec, failedCount);
            continue;
        }

        // collectRetentionEventSets() is newest-first, therefore delete tail only.
        for (int i = totalSets - 1; i >= kEventRetentionLimitPerCategory; --i) {
            const RetentionEventSet set = sets.at(i);

            qWarning() << "[Retention100][DELETE]"
                       << "category=" << spec.category
                       << "eventTime=" << set.eventDateTime.toString(Qt::ISODate)
                       << "path=" << set.path;

            /*
             * For event categories use a DB transaction around the DB-row removal.
             * If the physical folder cannot be removed, rollback the DB deletion so
             * the database never silently loses the event while its files remain.
             */
            int rowsDeletedForSet = 0;
            bool transactionStarted = false;

            if (spec.hasDatabaseRecords) {
                transactionStarted = db.transaction();
                if (!transactionStarted) {
                    failedCount++;
                    qWarning() << "[Retention100][BLOCK] cannot start DB transaction;"
                               << "keep event set:" << set.path
                               << db.lastError().text();
                    continue;
                }

                const int failureBeforeDb = failedCount;
                rowsDeletedForSet = deleteDatabaseRecordsInsideSet(db,
                                                                   spec.category,
                                                                   set.path,
                                                                   failedCount);

                if (failedCount != failureBeforeDb) {
                    db.rollback();
                    qWarning() << "[Retention100][BLOCK] DB cleanup failed;"
                               << "rollback and keep physical event set:" << set.path;
                    continue;
                }
            }

            if (!removeRetentionPhysicalSet(set,
                                            deletedFiles,
                                            deletedDirs,
                                            failedCount)) {
                if (transactionStarted) {
                    db.rollback();
                }
                qWarning() << "[Retention100] physical deletion failed;"
                           << "DB cleanup rolled back for:" << set.path;
                continue;
            }

            if (transactionStarted && !db.commit()) {
                /*
                 * Physical deletion already succeeded. A failed COMMIT is rare but
                 * must be visible; next daily run can clean the stale DB row.
                 */
                failedCount++;
                qWarning() << "[Retention100][DB] COMMIT failed after physical deletion:"
                           << set.path << db.lastError().text();
            } else {
                deletedDbRows += rowsDeletedForSet;
            }

            deletedSets++;
        }

        removeEmptyDateFoldersForRetention(spec.basePath, deletedDirs, failedCount);

        const int afterCount = collectRetentionEventSets(spec).size();
        qDebug() << "[Retention100][RESULT]"
                 << "category=" << spec.category
                 << "before=" << totalSets
                 << "after=" << afterCount
                 << "limit=" << kEventRetentionLimitPerCategory;

        deletedDbRows += cleanupMissingDatabaseRecordsForCategory(db, spec, failedCount);
    }

    if (openedHere) {
        db.close();
    }

    qWarning() << "[Retention100] daily retention check finished"
               << "deletedSets=" << deletedSets
               << "deletedFiles=" << deletedFiles
               << "deletedDirs=" << deletedDirs
               << "deletedDbRows=" << deletedDbRows
               << "failed=" << failedCount
               << "Pattern=EXCLUDED";
}

// void Database::deleteOldFilesAndRecords() {
//     qDebug() << "deleteOldFilesAndRecords";
//     if (!db.isOpen()) {
//         qDebug() << "Database is not open! Attempting to reconnect...";
//         if (!db.open()) {
//             qDebug() << "Failed to reconnect database:" << db.lastError().text();
//             return;
//         }
//     }

//     QSqlQuery query("SELECT full_path FROM event_records WHERE created_at < NOW() - INTERVAL 3 MONTH;");
//     qDebug() << "SELECT full_path FROM event_records WHERE created_at < NOW() - INTERVAL 3 MONTH;";
//     // Step 1: Select full paths of old files

//     if (!query.exec()) {
//         qDebug() << "Failed to fetch old file paths:" << query.lastError().text();
//         return;
//     }

//     // Step 2: Loop through the results and delete each file
//     while (query.next()) {
//         if (query.value("full_path").toString() == "") return;

//         QString fullPath = query.value("full_path").toString();
//         qDebug() << "File deleted:" << fullPath;
//         QDir dir(fullPath);

//         int lastSlash = fullPath.lastIndexOf("/");

//         // Extract the path before the last slash, this will give us the date folder path
//         QString parentDirectory = fullPath.left(lastSlash);

//         // Step 2: Get the parent directory of the 'date folder'
//         int secondLastSlash = parentDirectory.lastIndexOf("/");
//         if (secondLastSlash != -1) {
//             // This should give us the path up to the date folder, excluding the date folder itself
//             QString dateFolderPath = parentDirectory.left(secondLastSlash + 1);  // Up to and including the "2025-03-09" directory

//             QDir dir(dateFolderPath);

//             // Step 3: Check if the directory exists and delete it without affecting subdirectories
//             if (dir.exists()) {
//                 // Delete the folder at the date level
//                 if (dir.removeRecursively()) {
//                     qDebug() << "Deleted the date directory and its contents:" << dateFolderPath;
//                 } else {
//                     qDebug() << "Failed to delete the date directory:" << dateFolderPath;
//                 }
//             } else {
//                 qDebug() << "Date directory does not exist:" << dateFolderPath;
//             }
//         } else {
//             qDebug() << "Invalid path format!";
//         }
//     }

//     // Step 3: Delete old records from the database
//     QSqlQuery deleteQuery("DELETE FROM event_records WHERE created_at < NOW() - INTERVAL 3 MONTH;");

//     if (deleteQuery.exec()) {
//         qDebug() << "Old records deleted successfully from database!";
//     } else {
//         qDebug() << "Failed to delete old records:" << deleteQuery.lastError().text();
//     }
//     db.close();
// }

void Database::NewPatternFile(QString modeName, QString Name) {
    modeName = canonicalEventCategory(modeName);
    if (modeName.isEmpty()) {
        qWarning() << "[NewPatternFile] invalid modeName";
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd-HH:mm:ss.zzz");
    qDebug() << "Original Timestamp:" << timestamp;

    QString TimestampuseinFile = timestamp;
    TimestampuseinFile.replace(" ", "_");
    TimestampuseinFile.replace(".", "-");

    QString truncatedTimestamp = timestamp;
    int dotIndex = truncatedTimestamp.indexOf('.');
    if (dotIndex != -1) {
        truncatedTimestamp = truncatedTimestamp.left(dotIndex + 4);
    }

    QDateTime dateTime = QDateTime::fromString(truncatedTimestamp, "yyyy-MM-dd-HH:mm:ss.zzz");

    QString dateStr, timeStr;
    if (dateTime.isValid()) {
        dateStr = dateTime.toString("yyyy-MM-dd");
        timeStr = dateTime.toString("HH:mm:ss.zzz");
    } else {
        qDebug() << "Invalid timestamp format!";
    }

    QString timeWithoutMs = timeStr.section('.', 0, 0);
    timeWithoutMs.replace(":", "-");
    QString saveTimeInMysql = QString("%1-%2").arg(dateStr).arg(timeStr);

    QString folderPath = QDir::cleanPath(EVENT_PATH + modeName + QStringLiteral("/") + safePathPart(dateStr, QStringLiteral("date")));
    if (!isInsideAllowedEventWritePath(folderPath)) {
        qWarning() << "[NewPatternFile] unsafe folderPath, skip:" << folderPath;
        return;
    }
    qDebug() << "Surge folderPath:" << folderPath;

    QDir dir(folderPath);
    if (!dir.exists()) {
        if (dir.mkpath(".")) {
            qDebug() << "Directory Created Successfully";
        } else {
            qDebug() << "Failed to Create Directory!";
        }
    }

    folderPath = safeJoinEventPath(EVENT_PATH, modeName, dateStr, timeWithoutMs);
    if (folderPath.isEmpty()) {
        qWarning() << "[NewPatternFile] unsafe time folder, skip";
        return;
    }
    qDebug() << "Surge folderPath:" << folderPath;

    QDir dir2(folderPath);
    if (!dir2.exists()) {
        if (dir2.mkpath(".")) {
            qDebug() << "Directory Created Successfully";
        } else {
            qDebug() << "Failed to Create Directory!";
        }
    }
    qDebug() << "Tower and Distance:" << towerAndDistance->substation << towerAndDistance->direction << towerAndDistance->linenumber;

    int category_id = 0;
    QString fileName;

    modeName = canonicalEventCategory(modeName);
    if (modeName == "Pattern") {
        category_id = 1;
        fileName = safeEventFileName(Name.isEmpty() ? QStringLiteral("default_pattern") : Name, QStringLiteral("default_pattern"));
    } else {
        fileName = QStringLiteral("unknown_file");
    }

    QString fullPath = cleanAbsolutePath(dir2.filePath(fileName));
    if (!isInsideAllowedEventWritePath(fullPath)) {
        qWarning() << "[createPattern] unsafe fullPath, skip:" << fullPath;
        return;
    }
    qDebug() << "Final file path:" << fullPath;

    QFile file(fullPath);
    if (!file.exists()) {
        if (file.open(QIODevice::WriteOnly)) {
            qDebug() << "Empty CSV file created:" << fullPath;
            file.close();
        } else {
            qDebug() << "Failed to create CSV file:" << fullPath;
        }
    } else {
        qDebug() << "CSV file already exists:" << fullPath;
    }

    saveCsvFileAndUpdateDb(category_id, saveTimeInMysql, fileName, fullPath);
    getNewdatafromDB(modeName);
}

void Database::SavePatternFile(QString modeName, QString Name, QString event_datetime)
{
    const QString cleanCategory = canonicalEventCategory(modeName);
    if (cleanCategory.isEmpty()) {
        qWarning() << "[SavePatternFile] invalid category:" << modeName;
        return;
    }

    QString datefile;
    QString timefile;
    if (!parseEventDateTimeParts(event_datetime, datefile, timefile)) {
        qWarning() << "[SavePatternFile] invalid event datetime:" << event_datetime;
        return;
    }

    const QString safeName = safeEventFileName(Name, QStringLiteral("pattern.csv"));
    const QString filePath = safeJoinEventPath(EVENT_PATH, cleanCategory, datefile, timefile, safeName);
    if (filePath.isEmpty()) {
        qWarning() << "[SavePatternFile] unsafe file path";
        return;
    }

    qDebug() << "category:" << cleanCategory << timefile << datefile;
    qDebug() << "getCsvFile:" << filePath;

    QFile file(filePath);
    if (file.exists()) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.close();
            qDebug() << "Cleared existing CSV file:" << filePath;
        } else {
            qDebug() << "Failed to clear CSV file:" << filePath;
            return;
        }
    }

    writeCSV(cleanCategory, safeName, filePath, distAPat, voltAPat, distBPat, voltBPat, distCPat, voltCPat);
}

void Database::selectMasterModes() {
    if (!db.open()) {
        qDebug() << "Database connection failed:" << db.lastError().text();
        return;
    }

    QSqlQuery query("SELECT USER, IP_MASTER, IP_SLAVE FROM userMode");

    while (query.next()) {
        //        int id = query.value("id").toInt();
        QString user = query.value("USER").toString();
        QString ipMaster = query.value("IP_MASTER").toString();
        QString ipSlave = query.value("IP_SLAVE").toString();

        //        qDebug() << "ID:" << id
        // qWarning() << "User:" << user << "IP Master:" << ipMaster << "IP Slave:" << ipSlave;
        emit selectMasterMode(user, ipMaster, ipSlave);
    }

    db.close();
}

void Database::updateMasterMode(const QString &newUser, const QString &newIpMaster, const QString &newIpSlave) {
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }
    qDebug() << "updateMasterMode" << newUser;
    QSqlQuery query;
    query.prepare("UPDATE userMode SET USER = :user, IP_MASTER = :ipMaster, IP_SLAVE = :ipSlave WHERE id = 1");
    query.bindValue(":user", newUser);
    query.bindValue(":ipMaster", newIpMaster);
    query.bindValue(":ipSlave", newIpSlave);

    if (query.exec()) {
        qDebug() << "Record updated successfully!";
    } else {
        qDebug() << "Update failed:" << query.lastError().text();
    }
    db.close();
}

void Database::createSelectPattern() {
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }
    qDebug() << "createSelectPattern";

    QSqlQuery query;

    // Step 1: Create table if it doesn't exist
    query.prepare(
        "CREATE TABLE IF NOT EXISTS selectPattern ("
        "id INT AUTO_INCREMENT PRIMARY KEY, "
        "path VARCHAR(100), "
        "name VARCHAR(100)"
        ");");

    if (query.exec()) {
        qDebug() << "Table selectPattern checked/created successfully!";
    } else {
        qDebug() << "Table creation failed:" << query.lastError().text();
    }

    // Step 2: Check if the column 'datetimefile' exists
    query.prepare("SHOW COLUMNS FROM selectPattern LIKE 'datetimefile';");  // SQLite way to check columns, change for MySQL if needed
    if (!query.exec()) {
        qDebug() << "Failed to check table schema:" << query.lastError().text();
        return;
    }

    bool columnExists = false;
    while (query.next()) {
        if (query.value(1).toString() == "datetimefile") {  // Column names are in the second field (index 1)
            columnExists = true;
            break;
        }
    }

    if (!columnExists) {
        qDebug() << "Column 'datetimefile' does not exist. Adding it...";
        query.prepare("ALTER TABLE selectPattern ADD COLUMN datetimefile VARCHAR(100);");

        if (query.exec()) {
            qDebug() << "Column 'datetimefile' added successfully!";
        } else {
            qDebug() << "Failed to add column 'datetimefile':" << query.lastError().text();
        }
    } else {
        qDebug() << "Column 'datetimefile' already exists.";
    }

    // Step 3: Check if the table is empty
    query.prepare("SELECT COUNT(*) FROM selectPattern");
    if (!query.exec()) {
        qDebug() << "Failed to check table count:" << query.lastError().text();
        return;
    }

    query.next();
    int count = query.value(0).toInt();

    if (count == 0) {
        qDebug() << "Table is empty. Inserting data...";
        query.prepare("INSERT INTO selectPattern (path, name, datetimefile) VALUES ('', '', '')");

        if (query.exec()) {
            qDebug() << "Data inserted successfully!";
        } else {
            qDebug() << "Failed to insert data:" << query.lastError().text();
        }
    } else {
        qDebug() << "Table already has data. Skipping insert.";
    }

    db.close();
}

void Database::deleteSelectPattern()
{
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect to the database:" << db.lastError().text();
            return;
        }
    }

    qDebug() << "deleteSelectPattern function called.";

    QSqlQuery query(db);
    if (!query.exec("UPDATE selectPattern SET path = '', name = '', datetimefile = ''")) {
        qDebug() << "Failed to update selectPattern to empty values:" << query.lastError().text();
        db.close();
        return;
    }

    qDebug() << "All rows in selectPattern updated to empty values successfully.";

    // reset ค่าใน class ด้วย
    selectPatterPath = "";
    selectPatterName = "";
    datetimefile = "";

    emit patternSignal(selectPatterPath, selectPatterName, datetimefile);

    db.close();
}

void Database::SelectPattern() {
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect to the database:" << db.lastError().text();
            return;
        }
    }

    qDebug() << "SelectPattern function called.";

    QSqlQuery query;
    if (!query.exec("SELECT * FROM selectPattern")) {
        qDebug() << "Failed to execute SELECT query:" << query.lastError().text();
        db.close();
        return;
    }

    // Fetch data
    while (query.next()) {
        QString paths = query.value("path").toString();
        QString names = query.value("name").toString();
        QString date = query.value("datetimefile").toString();
        // Store the path (you might want to accumulate or process it)
        selectPatterPath = paths;
        selectPatterName = names;
        datetimefile = date;

        qWarning() << "Paths:" << paths << " selectPatter" << selectPatterPath << selectPatterName;
    }

    db.close();
}

void Database::updateSelectPattern(QString path, QString name, QString datetimefile)
{
    const QString safePath = path.trimmed();
    const QString safeName = name.trimmed();
    const QString safeDate = datetimefile.trimmed();

    if (safePath.isEmpty() || safeName.isEmpty() || safeDate.isEmpty() ||
        safePath.compare("null", Qt::CaseInsensitive) == 0 ||
        safeName.compare("null", Qt::CaseInsensitive) == 0 ||
        safeDate.compare("null", Qt::CaseInsensitive) == 0 ||
        safePath.compare("undefined", Qt::CaseInsensitive) == 0 ||
        safeName.compare("undefined", Qt::CaseInsensitive) == 0 ||
        safeDate.compare("undefined", Qt::CaseInsensitive) == 0) {

        qWarning() << "[updateSelectPattern] skip empty update"
                   << "path =" << safePath
                   << "name =" << safeName
                   << "datetimefile =" << safeDate;
        return;
    }

    QSqlQuery query(db);
    query.prepare("UPDATE selectPattern "
                  "SET path = :path, name = :name, datetimefile = :datetimefile");

    query.bindValue(":path", safePath);
    query.bindValue(":name", safeName);
    query.bindValue(":datetimefile", safeDate);

    if (!query.exec()) {
        qWarning() << "[updateSelectPattern] failed:" << query.lastError().text();
    }
}

void Database::createRangeLFL() {
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }
    qDebug() << "createSelectPattern";
    QSqlQuery query;
    query.prepare("CREATE TABLE IF NOT EXISTS RangeLFL (id INT(11) AUTO_INCREMENT PRIMARY KEY,count VARCHAR(20));");

    if (query.exec()) {
        qDebug() << "createRangeLFL successfully!";
    } else {
        qDebug() << "createRangeLFL failed:" << query.lastError().text();
    }

    // Step 1: Check if the table is empty
    query.prepare("SELECT COUNT(*) FROM RangeLFL");
    if (!query.exec()) {
        qDebug() << "Failed to check table count:" << query.lastError().text();
        return;
    }

    query.next();
    int count = query.value(0).toInt();

    if (count == 0) {
        qDebug() << "Table is empty. Inserting data...";

        // Step 2: Insert data if the table is empty
        query.prepare("INSERT INTO RangeLFL (count) VALUES ('')");

        if (query.exec()) {
            qDebug() << "Data inserted successfully!";
        } else {
            qDebug() << "Failed to insert data:" << query.lastError().text();
        }
    } else {
        qDebug() << "Table already has data. Skipping insert.";
    }

    db.close();
}

void Database::RangeLFL() {
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect to the database:" << db.lastError().text();
            return;
        }
    }

    qDebug() << "SelectPattern function called.";

    QSqlQuery query;
    if (!query.exec("SELECT * FROM RangeLFL")) {
        qDebug() << "Failed to execute SELECT query:" << query.lastError().text();
        db.close();
        return;
    }

    // Fetch data
    while (query.next()) {
        int count = query.value("count").toInt();
        // Store the path (you might want to accumulate or process it)
        lenghtFLF = count;

        qDebug() << "count:" << count;
    }

    db.close();
}

void Database::updateRangeLFL(QString c) {
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect database:" << db.lastError().text();
            return;
        }
    }
    qDebug() << "updateRangeLFL";
    QSqlQuery query;
    query.prepare("UPDATE RangeLFL SET count = :count WHERE id = 1;");
    query.bindValue(":count", c);

    if (query.exec()) {
        qDebug() << "updateRangeLFL successfully!";
    } else {
        qDebug() << "updateRangeLFL failed:" << query.lastError().text();
    }

    db.close();
}

void Database::selectMarginSettingParameter() {
    if (!db.isOpen()) {
        qDebug() << "Database is not open! Attempting to reconnect...";
        if (!db.open()) {
            qDebug() << "Failed to reconnect to the database:" << db.lastError().text();
            return;
        }
    }

    qDebug() << "SelectPattern function called.";

    QSqlQuery query;

    // Step 1: Check if the table is empty
    if (query.exec("SELECT COUNT(*) FROM MarginSettingParameter")) {
        int count=0;
        if (query.next()) {
            count = query.value(0).toInt();
            qDebug() << "LOPP MarginSettingParameter count:" << count;
        }
        qDebug() << "MarginSettingParameter count:" << count;
        if (count == 0) {
            // Step 2: Insert default values
            QSqlQuery insertQuery;
            insertQuery.prepare("INSERT INTO MarginSettingParameter (margin,valueVoltage,focusIndex,PHASE) VALUES (3,300,0,'A')");

            if (!insertQuery.exec()) {
                qDebug() << "Insert failed:" << insertQuery.lastError().text();
            } else {
                qDebug() << "Default values inserted into MarginParameter.";
            }

            insertQuery.prepare("INSERT INTO MarginSettingParameter (margin,valueVoltage,focusIndex,PHASE) VALUES (3,300,0,'B')");
            if (!insertQuery.exec()) {
                qDebug() << "Insert failed:" << insertQuery.lastError().text();
            } else {
                qDebug() << "Default values inserted into MarginParameter.";
            }

            insertQuery.prepare("INSERT INTO MarginSettingParameter (margin,valueVoltage,focusIndex,PHASE) VALUES (3,300,0,'C')");
            if (!insertQuery.exec()) {
                qDebug() << "Insert failed:" << insertQuery.lastError().text();
            } else {
                qDebug() << "Default values inserted into MarginParameter.";
            }

        } else {
            qDebug() << "Table already has" << count << "records.";
        }
    } else {
        qDebug() << "Count query failed:" << query.lastError().text();
    }


    if (!query.exec("SELECT * FROM MarginSettingParameter")) {
        qDebug() << "Failed to execute SELECT query:" << query.lastError().text();
        db.close();
        return;
    }
    // Fetch data
    while (query.next()) {
        int margin = query.value("margin").toInt();
        QString phase = query.value("PHASE").toString();
        // Store the path (you might want to accumulate or process it)
        if (phase == "A") {
            lenghtMarginA = margin;
        }
        if (phase == "B") {
            lenghtMarginB = margin;
        }
        if (phase == "C") {
            lenghtMarginC = margin;
        }
    }
    qDebug() << "lenghtMarginA:" << lenghtMarginA << " lenghtMarginB:" << lenghtMarginB << " lenghtMarginC:" << lenghtMarginC;

    QString cmd = QString("SELECT * FROM MarginTableA LIMIT %1;").arg(lenghtMarginA);
    if (!query.exec(cmd)) {
        qDebug() << "Failed to execute SELECT query:" << query.lastError().text();
        db.close();
        return;
    }
    // Fetch data
    int p = 0;
    while (query.next()) {
        int value = query.value("value of margin").toInt();
        valueOfMarginA[p] = value;
        // Store the path (you might want to accumulate or process it)
        qDebug() << "marginA value:" << value;
        p++;
    }

    cmd = QString("SELECT * FROM MarginTableB LIMIT %1;").arg(lenghtMarginB);
    if (!query.exec(cmd)) {
        qDebug() << "Failed to execute SELECT query:" << query.lastError().text();
        db.close();
        return;
    }
    // Fetch data
    p = 0;
    while (query.next()) {
        int value = query.value("value of margin").toInt();
        valueOfMarginB[p] = value;
        // Store the path (you might want to accumulate or process it)
        qDebug() << "marginB value:" << value;
        p++;
    }

    cmd = QString("SELECT * FROM MarginTableC LIMIT %1;").arg(lenghtMarginC);
    if (!query.exec(cmd)) {
        qDebug() << "Failed to execute SELECT query:" << query.lastError().text();
        db.close();
        return;
    }
    // Fetch data
    p = 0;
    while (query.next()) {
        int value = query.value("value of margin").toInt();
        valueOfMarginC[p] = value;
        // Store the path (you might want to accumulate or process it)
        qDebug() << "marginC value:" << value;
        p++;
    }
    db.close();
}

