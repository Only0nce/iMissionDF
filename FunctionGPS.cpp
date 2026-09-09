#include "PLCServer.h"
#include <QTcpSocket>
#include <QMutex>

namespace {

QMutex g_ftpGuardMutex;
QString g_ftpBlockedHost;
qint64 g_ftpBlockedUntilMs = 0;
int g_ftpFailCount = 0;

QString extractLftpHostFromCommand(const QString &commandLine)
{
    /*
     * รองรับ command รูปแบบ:
     * lftp -u user,pass 192.168.10.150 -e '...'
     */
    const QRegularExpression re(
        QStringLiteral("^\\s*lftp\\s+-u\\s+\\S+\\s+(\\S+)\\s+-e\\s+[\\s\\S]+$")
        );

    const QRegularExpressionMatch match = re.match(commandLine.trimmed());

    if (!match.hasMatch()) {
        return QString();
    }

    QString host = match.captured(1).trimmed();

    if ((host.startsWith('\'') && host.endsWith('\'')) ||
        (host.startsWith('"') && host.endsWith('"'))) {
        host = host.mid(1, host.size() - 2);
    }

    return host.trimmed();
}

bool isTcpPortOpen(const QString &host, quint16 port, int timeoutMs)
{
    if (host.trimmed().isEmpty()) {
        return false;
    }

    QTcpSocket socket;
    socket.connectToHost(host.trimmed(), port);

    if (!socket.waitForConnected(timeoutMs)) {
        qWarning() << "[FTP-GUARD] host not reachable:"
                   << host
                   << "port =" << port
                   << "error =" << socket.errorString();
        return false;
    }

    socket.disconnectFromHost();
    return true;
}

bool isFtpCooldownActive(const QString &host)
{
    QMutexLocker locker(&g_ftpGuardMutex);

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    if (g_ftpBlockedHost == host && nowMs < g_ftpBlockedUntilMs) {
        qWarning() << "[FTP-GUARD] skip FTP, cooldown active:"
                   << "host =" << host
                   << "remainMs =" << (g_ftpBlockedUntilMs - nowMs)
                   << "failCount =" << g_ftpFailCount;
        return true;
    }

    return false;
}

void blockFtpHost(const QString &host, int cooldownMs)
{
    QMutexLocker locker(&g_ftpGuardMutex);

    g_ftpBlockedHost = host;
    g_ftpBlockedUntilMs = QDateTime::currentMSecsSinceEpoch() + cooldownMs;
    g_ftpFailCount++;

    qWarning() << "[FTP-GUARD] block FTP host:"
               << host
               << "cooldownMs =" << cooldownMs
               << "failCount =" << g_ftpFailCount;
}

void clearFtpBlockIfMatch(const QString &host)
{
    QMutexLocker locker(&g_ftpGuardMutex);

    if (g_ftpBlockedHost == host) {
        g_ftpBlockedHost.clear();
        g_ftpBlockedUntilMs = 0;
        g_ftpFailCount = 0;

        qDebug() << "[FTP-GUARD] FTP host recovered:" << host;
    }
}

bool ensureDirExists(const QString &path)
{
    return QDir().mkpath(QDir::cleanPath(path));
}

QStringList redactedArgumentsForLog(const QString &program, QStringList arguments)
{
    if (program == QStringLiteral("lftp")) {
        const int userIndex = arguments.indexOf(QStringLiteral("-u"));
        if (userIndex >= 0 && userIndex + 1 < arguments.size()) {
            QString credential = arguments.at(userIndex + 1);
            const int commaIndex = credential.indexOf(QLatin1Char(','));
            if (commaIndex >= 0) {
                credential = credential.left(commaIndex + 1) + QStringLiteral("******");
                arguments[userIndex + 1] = credential;
            }
        }
    }
    return arguments;
}

bool runProcessChecked(const QString &program,
                       const QStringList &arguments,
                       int timeoutMs = 30000,
                       bool logError = true)
{
    const QStringList logArguments = redactedArgumentsForLog(program, arguments);

    QProcess process;
    process.start(program, arguments);

    if (!process.waitForStarted(5000)) {
        if (logError) {
            qWarning() << "[SafeProcess] failed to start" << program << logArguments
                       << process.errorString();
        }
        return false;
    }

    if (!process.waitForFinished(timeoutMs)) {
        if (logError) {
            qWarning() << "[SafeProcess] timeout" << program << logArguments;
        }
        process.kill();
        process.waitForFinished(3000);
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (logError) {
            qWarning() << "[SafeProcess] failed" << program << logArguments
                       << "exitCode=" << process.exitCode()
                       << "stderr=" << QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        }
        return false;
    }

    return true;
}

QString redactedCommand(QString command)
{
    command.replace(QRegularExpression(QStringLiteral("(-u\\s+[^,\\s]+,)([^\\s]+)")), QStringLiteral("\\1******"));
    return command;
}

QString normalizeLftpScript(QString script)
{
    script = script.trimmed();

    if ((script.startsWith(QLatin1Char('\'')) && script.endsWith(QLatin1Char('\''))) ||
        (script.startsWith(QLatin1Char('"')) && script.endsWith(QLatin1Char('"')))) {
        script = script.mid(1, script.size() - 2).trimmed();
    }

    // Legacy command strings had: put <local> -o <remote> bye
    // For lftp -e, bye must be its own command, otherwise it can be parsed as
    // another put argument. Convert only the final trailing " bye" into "; bye".
    if (!script.contains(QRegularExpression(QStringLiteral(";\\s*bye\\s*$")))) {
        script.replace(QRegularExpression(QStringLiteral("\\s+bye\\s*$")), QStringLiteral("; bye"));
    }

    // Keep the legacy lftp mkdir behavior exactly as the old system() command.
    // Some embedded FTP servers reject lftp's recursive mkdir -p mode even when
    // the normal sequential mkdir command works.

    return script;
}

QStringList splitLftpScriptCommands(const QString &script)
{
    QStringList commands;
    QString current;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool escaped = false;

    for (const QChar ch : script) {
        if (escaped) {
            current.append(ch);
            escaped = false;
            continue;
        }

        if (ch == QLatin1Char('\\') && inDoubleQuote) {
            current.append(ch);
            escaped = true;
            continue;
        }

        if (ch == QLatin1Char('\'') && !inDoubleQuote) {
            inSingleQuote = !inSingleQuote;
            current.append(ch);
            continue;
        }

        if (ch == QLatin1Char('"') && !inSingleQuote) {
            inDoubleQuote = !inDoubleQuote;
            current.append(ch);
            continue;
        }

        if (ch == QLatin1Char(';') && !inSingleQuote && !inDoubleQuote) {
            const QString command = current.trimmed();
            if (!command.isEmpty()) {
                commands.append(command);
            }
            current.clear();
            continue;
        }

        current.append(ch);
    }

    const QString command = current.trimmed();
    if (!command.isEmpty()) {
        commands.append(command);
    }

    return commands;
}

QString unquoteLftpValue(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2) {
        const QChar first = value.at(0);
        const QChar last = value.at(value.size() - 1);
        if ((first == QLatin1Char('\'') && last == QLatin1Char('\'')) ||
            (first == QLatin1Char('"') && last == QLatin1Char('"'))) {
            value = value.mid(1, value.size() - 2);
        }
    }

    value.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
    value.replace(QStringLiteral("\\\""), QStringLiteral("\""));
    return value.trimmed();
}

QString lftpPathArgument(const QString &path)
{
    QString cleanPath = path.trimmed();
    if (cleanPath.isEmpty()) {
        return cleanPath;
    }

    // Keep the same plain path format as the old command when it is already safe.
    // Quote only when needed, because some older FTP/lftp combinations are picky.
    if (!cleanPath.contains(QRegularExpression(QStringLiteral("[\\s;\\'\"]")))) {
        return cleanPath;
    }

    cleanPath.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    cleanPath.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(cleanPath);
}

bool parseLftpMkdirCommand(const QString &command, QString *path)
{
    QString trimmed = command.trimmed();
    if (!trimmed.startsWith(QStringLiteral("mkdir "))) {
        return false;
    }

    QString rest = trimmed.mid(QStringLiteral("mkdir").size()).trimmed();
    if (rest.startsWith(QStringLiteral("-p "))) {
        rest = rest.mid(2).trimmed();
    }

    if (rest.isEmpty() || rest.startsWith(QLatin1Char('-'))) {
        return false;
    }

    *path = unquoteLftpValue(rest);
    return !path->isEmpty();
}

bool isLftpSetCommand(const QString &command)
{
    const QString trimmed = command.trimmed();
    return trimmed == QStringLiteral("set") || trimmed.startsWith(QStringLiteral("set "));
}

bool isLftpByeCommand(const QString &command)
{
    const QString trimmed = command.trimmed().toLower();
    return trimmed == QStringLiteral("bye") || trimmed == QStringLiteral("exit") || trimmed == QStringLiteral("quit");
}

QString buildLftpScript(const QStringList &setupCommands, const QString &mainCommand)
{
    QStringList commands = setupCommands;
    commands.append(mainCommand);
    commands.append(QStringLiteral("bye"));
    return commands.join(QStringLiteral("; "));
}

bool runLftpScript(const QString &credential,
                   const QString &host,
                   const QString &script,
                   int timeoutMs,
                   bool logError = true)
{
    const QStringList args{
        QStringLiteral("-u"),
        credential,
        QStringLiteral("-e"),
        script,
        host
    };

    return runProcessChecked(QStringLiteral("lftp"), args, timeoutMs, logError);
}

bool runLftpMkdirScriptWithExistenceCheck(const QString &credential,
                                          const QString &host,
                                          const QString &script,
                                          int timeoutMs,
                                          bool *handled)
{
    if (handled) {
        *handled = false;
    }

    const QStringList commands = splitLftpScriptCommands(script);
    QStringList setupCommands;
    QStringList mkdirPaths;

    for (const QString &command : commands) {
        QString mkdirPath;
        if (isLftpSetCommand(command)) {
            setupCommands.append(command);
        } else if (parseLftpMkdirCommand(command, &mkdirPath)) {
            mkdirPaths.append(mkdirPath);
        } else if (isLftpByeCommand(command)) {
            // ignore; buildLftpScript() adds bye again
        } else {
            return false; // Not a mkdir-only script; run it normally.
        }
    }

    if (mkdirPaths.isEmpty()) {
        return false;
    }

    if (handled) {
        *handled = true;
    }

    bool ok = true;
    for (const QString &path : mkdirPaths) {
        const QString pathArg = lftpPathArgument(path);
        const QString checkScript = buildLftpScript(setupCommands, QStringLiteral("cd %1").arg(pathArg));

        if (runLftpScript(credential, host, checkScript, timeoutMs, false)) {
            qDebug() << "[SafeProcess] FTP directory exists, skip mkdir:" << path;
            continue;
        }

        const QString mkdirScript = buildLftpScript(setupCommands, QStringLiteral("mkdir %1").arg(pathArg));
        if (runLftpScript(credential, host, mkdirScript, timeoutMs, true)) {
            qDebug() << "[SafeProcess] FTP directory created:" << path;
            continue;
        }

        // One final check: some FTP servers create the folder but still return a
        // failing status. If cd works now, treat it as success and avoid blocking
        // the following upload.
        if (runLftpScript(credential, host, checkScript, timeoutMs, false)) {
            qWarning() << "[SafeProcess] FTP mkdir returned error, but directory now exists:" << path;
            continue;
        }

        qWarning() << "[SafeProcess] FTP directory create failed:" << path;
        ok = false;
    }

    return ok;
}

bool runLegacyLftpCommandLine(const QString &commandLine, int timeoutMs)
{
    const QString trimmed = commandLine.trimmed();

    // Expected legacy format:
    // lftp -u user,pass host -e 'set ...; cd ...; put ... -o ...; bye'
    // Do not use QProcess::splitCommand() here because Qt 5 does not reliably
    // keep single-quoted -e scripts as one argument.
    const QRegularExpression legacyRe(
        QStringLiteral("^lftp\\s+-u\\s+(\\S+)\\s+(\\S+)\\s+-e\\s+([\\s\\S]+)$"));
    const QRegularExpressionMatch match = legacyRe.match(trimmed);

    if (!match.hasMatch()) {
        return false;
    }

    const QString credential = match.captured(1).trimmed();
    const QString host = match.captured(2).trimmed();
    const QString script = normalizeLftpScript(match.captured(3));

    if (credential.isEmpty() || host.isEmpty() || script.isEmpty()) {
        qWarning() << "[SafeProcess] invalid lftp command" << redactedCommand(commandLine);
        return false;
    }

    bool mkdirScriptHandled = false;
    const bool mkdirScriptOk = runLftpMkdirScriptWithExistenceCheck(credential, host, script, timeoutMs, &mkdirScriptHandled);
    if (mkdirScriptHandled) {
        return mkdirScriptOk;
    }

    return runLftpScript(credential, host, script, timeoutMs, true);
}

bool runCommandLineNoShell(const QString &commandLine,
                           const QStringList &allowedPrograms = QStringList{QStringLiteral("lftp")},
                           int timeoutMs = 45000)
{
    const QString trimmed = commandLine.trimmed();
    if (trimmed.isEmpty()) {
        return true;
    }

    if (trimmed.startsWith(QStringLiteral("lftp "))) {
        return runLegacyLftpCommandLine(trimmed, timeoutMs);
    }

    const QStringList parts = QProcess::splitCommand(trimmed);
    if (parts.isEmpty()) {
        qWarning() << "[SafeProcess] empty command after split";
        return false;
    }

    const QString program = parts.first();
    if (!allowedPrograms.contains(program)) {
        qWarning() << "[SafeProcess] blocked command" << redactedCommand(commandLine);
        return false;
    }

    return runProcessChecked(program, parts.mid(1), timeoutMs);
}

void ensureRequiredPath(const QString &label, const QString &path)
{
    QDir dir(path);
    if (dir.exists()) {
        qDebug() << label << "Directory exists:" << path;
        return;
    }

    if (ensureDirExists(path)) {
        qDebug() << label << "Directory created:" << path;
    } else {
        qWarning() << label << "failed to create directory:" << path;
    }
}

bool safeRemoveRegularFile(const QString &path)
{
    const QFileInfo info(QDir::cleanPath(path));
    if (!info.exists()) {
        return true;
    }
    if (!info.isFile() || info.isSymLink()) {
        qWarning() << "[SafeFile] blocked remove:" << info.absoluteFilePath();
        return false;
    }
    if (!QFile::remove(info.absoluteFilePath())) {
        qWarning() << "[SafeFile] remove failed:" << info.absoluteFilePath();
        return false;
    }
    return true;
}

void removeFilesInFixedDir(const QString &dirPath)
{
    QDir dir(QDir::cleanPath(dirPath));
    if (!dir.exists()) {
        return;
    }

    const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name);
    for (const QFileInfo &fileInfo : files) {
        safeRemoveRegularFile(fileInfo.absoluteFilePath());
    }
}

} // namespace

bool PLCServer::runFtpCommandNow(QString command, int timeoutMs)
{
    const QString trimmed = command.trimmed();

    // qWarning() << "runFtpCommandNow::" << trimmed;

    if (trimmed.isEmpty()) {
        return true;
    }

    const QString host = extractLftpHostFromCommand(trimmed);

    /*
     * ถ้า FTP ล่ม/No route แล้ว ให้ skip ทันที
     * ไม่ให้ lftp loop block งาน Monitor/WebSocket
     */
    if (!host.isEmpty()) {
        if (isFtpCooldownActive(host)) {
            return false;
        }

        /*
         * เช็ค port 21 ก่อนเรียก lftp จริง
         * timeout สั้น ๆ พอ กันค้าง
         */
        if (!isTcpPortOpen(host, 21, 800)) {
            blockFtpHost(host, 60 * 1000);
            return false;
        }
    }

    /*
     * กัน lftp ค้างนานเกินไป
     * ถึง caller จะส่ง 60000/90000 ms มา ก็ cap เหลือไม่เกิน 8000 ms
     */
    const int safeTimeoutMs = qBound(1000, timeoutMs, 8000);

    const bool ok = runCommandLineNoShell(
        trimmed,
        QStringList{QStringLiteral("lftp")},
        safeTimeoutMs
        );

    if (!host.isEmpty()) {
        if (ok) {
            clearFtpBlockIfMatch(host);
        } else {
            blockFtpHost(host, 60 * 1000);
        }
    }

    return ok;
}

void PLCServer::SocketClientError() { qDebug() << "SocketClientError"; }

void PLCServer::manageDataClient(QString msgs) {
    QByteArray br = msgs.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);
    QJsonObject obj = doc.object();
    QJsonObject command = doc.object();
    // qDebug() << "msgs" << msgs;
    if (obj["menuID"] == "chrony_sources_list") {
        // if (obj["id"] == 0) {
        //     QString sources = obj["chronySources"].toString();
        //     int lastRx = obj["lastRx"].toInt();
        //     int stratum = obj["Stratum"].toInt();

        //     // 1) PPS ถูกเลือกเป็น reference
        //     if (sources.contains("*,PPS")) {
        //         gps->PPS = true;
        //         // qWarning() << "[GPS OK] PPS is selected as reference";
        //     }
        //     // 2) PPS มี แต่ไม่ได้ถูกเลือก (* ไม่อยู่หน้า PPS)
        //     else if (sources.contains("PPS") && !sources.contains("*,PPS")) {
        //         gps->PPS = false;
        //         qWarning() << "[GPS WARN] PPS detected but not used as ref";
        //     }
        //     // 3) ไม่เจอ PPS เลย
        //     else {
        //         gps->PPS = false;
        //         qWarning() << "[GPS FAIL] PPS not detected";
        //     }
        //     // ตรวจเพิ่มจาก lastRx (ถ้านานเกินไปถือว่าสัญญาณหาย)
        //     if (lastRx > 60*10) {
        //         qWarning() << "[GPS LOST] No signal for" << lastRx << "seconds";
        //         gps->PPS = false;
        //     }
        //     // ตรวจ Stratum (0 = PPS, 1 = NMEA)
        //     if (stratum > 1) {
        //         qWarning() << "[GPS NOT SYNC] Stratum =" << stratum;
        //         gps->PPS = false;
        //     }
        //     qDebug() << "ตรวจเพิ่มจาก lastRx:" << lastRx;
        // }
    }
     else if (obj["menuID"] == "GPS_Data") {
        // qDebug() << "GPS_SatUse:" << obj["GPS_SatUse"].toInt();
        if (obj["GPS_SatUse"].toInt() > 3) {
            // GPS_Count=0;
            gps->GPS_Data = true;
            gps->PPS = true;
            // qWarning() << "....................GPS_SatUse TRUE....................";
        } else {
            // GPS_Count++;
            gps->GPS_Data = false;
            gps->PPS = false;
            // qDebug() << "....................GPS_SatUse FALSE....................";
        }
        // if(GPS_Count >= 60*10){
        //     gps->GPS_Data = false;
        //     GPS_Count = 1000;
        //     qWarning() << "....................GPS_SatUse TRUE....................";
        // // }
    }
}

void PLCServer::manageDataClientPLC(QString msgs){
    QByteArray br = msgs.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(br);
    QJsonObject obj = doc.object();
    QJsonObject command = doc.object();
    if (obj["objectName"].toString() == "keepAlive") {
        if (obj["HwName"].toString() == "OpenPLCSever") {
            // qWarning() << "OpenPLCSever keepAlive";
            plcServer_count = 0;
            //            qDebug() << "receive from slave";
        }
    } else if (obj["menuID"].toString() == "RelayTest") {
        // qWarning() << "menuID RelayTest";
        sendDIO = true;
    } else if(obj["menuID"].toString() == "RelayTestStop"){
        // qWarning() << "menuID RelayTestStop";
        sendDIO = false;
    }
}

void PLCServer::assignThreshold(double A, double B, double C) {
    thresholdA = A;
    thresholdB = B;
    thresholdC = C;
    qDebug() << "thresholdA B C" << thresholdA << thresholdB << thresholdC << A << B << C;
}

void PLCServer::assignGetSettingDisplay(double SAG, double Sampling_rate, double DistanceStart, double DistanceStop, double FullDistance) {
    sagFactor = SAG;                  // SAG factor
    samplingRate = Sampling_rate;     // Sampling rate (meters per sample)
    distanceToStart = DistanceStart;  // ระยะตั้งต้น (เมตร)
    distanceToShow = DistanceStop;    // ระยะปลายทาง (เมตร)
    fulldistance = FullDistance;
    SetupEquipment->Distance = QString::number(FullDistance); //FullDistance;
    // qWarning() << "sagFactor" << sagFactor << " samplingRate" << samplingRate << " distanceToStart" << distanceToStart << " distanceToShow" << distanceToShow << " fulldistance" << fulldistance;
}

// bool PLCServer::findMaxEachIndex(QString phase,QList<QList<double>> &voltList, QList<QList<float>> &disList, QList<QList<double>> &MaxVoltList, QList<QList<float>> &MaxDisList, std::vector<std::pair<float, float>> &result) {
//     int size = voltList.size();
//     if (size < 1 || disList.size() < 1) {
//         qWarning() << "No voltage lists provided.";
//         return false;
//     }
//     qDebug() << "numLists:::" << size ;
//     //    int numElements = voltageList[0].size(); // Assuming all lists have the same size
//     int numLists = voltList[0].size();

//     // Find max value at each index
//     QList<double> maxValues;
//     for (int i = 0; i < numLists; i++) {
//         double maxVal = -FLT_MAX;  // Use the minimum float value

//         // Find the maximum value for this index across all lists
//         for (const QList<double> &list : voltList) {
//             if (list.size() > i) {
//                 maxVal = qMax(maxVal, list[i]);
//             } else {
//                 qWarning() << "Index out of range in list at index" << i;
//             }
//         }

//         maxValues.append(maxVal);
//     }
//     // Print maximum values at each index
//     qDebug() << "Maximum values at each index: phase" << phase;
//     // for (int i = 0; i < maxValues.size(); i++) {
//     //     qDebug() << "Index" << i << ":" << maxValues[i];
//     // }

//     // MaxVoltList.append(maxValues);
//     // MaxDisList.append(disList[0]);

//     // if(phase == "A"){
//     //     distanceArrayAPattern.clear();  // m → km
//     //     voltageArrayAPattern.clear();
//     //     distanceArrayAPatternbkup.clear();  // m → km
//     //     voltageArrayAPatternbkup.clear();
//     // }
//     // else if(phase == "B"){
//     //     distanceArrayBPattern.clear();  // m → km
//     //     voltageArrayBPattern.clear();
//     //     distanceArrayBPatternbkup.clear();  // m → km
//     //     voltageArrayBPatternbkup.clear();
//     // }
//     // else if(phase == "C"){
//     //     distanceArrayCPattern.clear();  // m → km
//     //     voltageArrayCPattern.clear();
//     //     distanceArrayCPatternbkup.clear();  // m → km
//     //     voltageArrayCPatternbkup.clear();
//     // }

//     QJsonObject mainObject;
//     QJsonArray dist, volt;

//     for (int i = 0; i < numLists; ++i) {
//         dist.push_back(disList[0][i] / 1000.0);  // m → km
//         volt.push_back(maxValues[i]);
//         if(phase == "A"){
//             distanceArrayAPattern.push_back(disList[0][i] / 1000.0);  // m → km
//             voltageArrayAPattern.push_back(maxValues[i]);
//             distanceArrayAPatternbkup.push_back(disList[0][i] / 1000.0);  // m → km
//             voltageArrayAPatternbkup.push_back(maxValues[i]);
//         }
//         else if(phase == "B"){
//             distanceArrayBPattern.push_back(disList[0][i] / 1000.0);  // m → km
//             voltageArrayBPattern.push_back(maxValues[i]);
//             distanceArrayBPatternbkup.push_back(disList[0][i] / 1000.0);  // m → km
//             voltageArrayBPatternbkup.push_back(maxValues[i]);
//         }
//         else if(phase == "C"){
//             distanceArrayCPattern.push_back(disList[0][i] / 1000.0);  // m → km
//             voltageArrayCPattern.push_back(maxValues[i]);
//             distanceArrayCPatternbkup.push_back(disList[0][i] / 1000.0);  // m → km
//             voltageArrayCPatternbkup.push_back(maxValues[i]);
//         }
//     }

//     // RecalculateWithMarginManual(dist, volt, "A");
//     // qDebug() << "RecalculateWithMarginManual:" << volt << dist << interlockPattern << " phase" << phase;

//     if(phase == "A"){
//         if (patternSelected) {
//             calFLF(dist, volt, "A");
//         }
//         RecalculateWithMarginManual(dist, volt, "A");
//     }
//     else if(phase == "B"){
//         if (patternSelected) {
//             calFLF(dist, volt, "B");
//         }
//         RecalculateWithMarginManual(dist, volt, "B");
//     }
//     else if(phase == "C"){
//         if (patternSelected) {
//             calFLF(dist, volt, "C");
//         }
//         RecalculateWithMarginManual(dist, volt, "C");
//     }
//     // mainObject.insert("distance", dist);
//     // mainObject.insert("voltage", volt);

//     // QJsonDocument jsonDoc(mainObject);
//     // QString raw_data = jsonDoc.toJson(QJsonDocument::Compact);
//     // emit sendToSocketPLC(raw_data);

//     //    for (const auto& pair : result) {
//     //        qDebug() << "Pair: (" << pair.first << ", " << pair.second << ")";
//     //    }
//     //    reSamplingNormalizationA(resultMaxList);
//     return true;
// }

bool PLCServer::findMaxEachIndex(QString phase,
                                 QList<QList<double>> &voltList,
                                 QList<QList<float>> &disList,
                                 QList<QList<double>> &MaxVoltList,
                                 QList<QList<float>> &MaxDisList,
                                 std::vector<std::pair<float, float>> &result)
{
    Q_UNUSED(MaxVoltList)
    Q_UNUSED(MaxDisList)
    Q_UNUSED(result)

    phase = phase.trimmed().toUpper();

    if (phase != "A" && phase != "B" && phase != "C") {
        qWarning() << "[findMaxEachIndex] invalid phase:" << phase;
        return false;
    }

    int size = voltList.size();
    if (size < 1 || disList.size() < 1) {
        qWarning() << "[findMaxEachIndex] No voltage/distance lists provided."
                   << "phase" << phase;
        return false;
    }

    if (voltList[0].isEmpty() || disList[0].isEmpty()) {
        qWarning() << "[findMaxEachIndex] first voltage/distance list is empty."
                   << "phase" << phase;
        return false;
    }

    int numLists = voltList[0].size();

    if (disList[0].size() < numLists) {
        qWarning() << "[findMaxEachIndex] distance list shorter than voltage list."
                   << "phase" << phase
                   << "volt size" << numLists
                   << "dist size" << disList[0].size();
        return false;
    }

    qDebug() << "[findMaxEachIndex]"
             << "phase" << phase
             << "num voltage lists" << size
             << "num points" << numLists;

    /*
     * ใช้ local flag แทนการเพิ่ม parameter
     *
     * สำคัญ:
     * - patternSelected = มี Pattern ถูกเลือกอยู่ ไม่ได้แปลว่ากำลังสร้าง Pattern ใหม่
     * - ดังนั้นอย่าใช้ patternSelected เป็นเงื่อนไขเขียนทับ Pattern storage
     *
     * ให้เขียนทับ global Pattern เฉพาะตอนระบบอยู่ใน flow Pattern เท่านั้น
     */
    const bool isPatternMode =
        modeName.contains("Pattern", Qt::CaseInsensitive) ||
        interlockPattern ||
        interlockPressPattern;

    // Find max value at each index
    QList<double> maxValues;
    maxValues.reserve(numLists);

    for (int i = 0; i < numLists; i++) {
        double maxVal = -DBL_MAX;
        bool hasValue = false;

        for (const QList<double> &list : voltList) {
            if (list.size() > i) {
                maxVal = qMax(maxVal, list[i]);
                hasValue = true;
            } else {
                qWarning() << "[findMaxEachIndex] Index out of range"
                           << "phase" << phase
                           << "index" << i
                           << "list size" << list.size();
            }
        }

        if (!hasValue) {
            qWarning() << "[findMaxEachIndex] no valid value at index"
                       << i << "phase" << phase;
            return false;
        }

        maxValues.append(maxVal);
    }

    qDebug() << "Maximum values at each index: phase" << phase;

    QJsonArray dist;
    QJsonArray volt;

    /*
     * เก็บข้อมูลใหม่ไว้ใน local ก่อน
     * ห้าม push_back เข้า global ทันที
     * เพื่อป้องกัน:
     * 1. ข้อมูลซ้ำ
     * 2. ข้อมูลเก่าหายถ้าคำนวณไม่สำเร็จ
     * 3. global ถูกแก้โดยไม่ใช่ New Pattern
     */
    QVector<double> newDistancePattern;
    QVector<double> newVoltagePattern;

    newDistancePattern.reserve(numLists);
    newVoltagePattern.reserve(numLists);

    for (int i = 0; i < numLists; ++i) {
        const double distanceKm = disList[0][i] / 1000.0;
        const double voltageVal = maxValues[i];

        dist.push_back(distanceKm);
        volt.push_back(voltageVal);

        newDistancePattern.push_back(distanceKm);
        newVoltagePattern.push_back(voltageVal);
    }

    /*
     * ตรงนี้คือจุดสำคัญ:
     * ถ้าเป็น Pattern mode จริง ค่อย assign ทับ global
     * ไม่ใช้ clear + push_back แล้ว
     */
    if (isPatternMode) {
        if (phase == "A") {
            distanceArrayAPattern = newDistancePattern;
            voltageArrayAPattern = newVoltagePattern;

            distanceArrayAPatternbkup = newDistancePattern;
            voltageArrayAPatternbkup = newVoltagePattern;
        }
        else if (phase == "B") {
            distanceArrayBPattern = newDistancePattern;
            voltageArrayBPattern = newVoltagePattern;

            distanceArrayBPatternbkup = newDistancePattern;
            voltageArrayBPatternbkup = newVoltagePattern;
        }
        else if (phase == "C") {
            distanceArrayCPattern = newDistancePattern;
            voltageArrayCPattern = newVoltagePattern;

            distanceArrayCPatternbkup = newDistancePattern;
            voltageArrayCPatternbkup = newVoltagePattern;
        }

        qWarning() << "[findMaxEachIndex] update Pattern storage"
                   << "phase" << phase
                   << "points" << newDistancePattern.size()
                   << "modeName" << modeName
                   << "interlockPattern" << interlockPattern
                   << "interlockPressPattern" << interlockPressPattern;
    }
    else {
        qWarning() << "[findMaxEachIndex] skip update Pattern storage"
                   << "phase" << phase
                   << "reason: not Pattern mode"
                   << "modeName" << modeName
                   << "interlockPattern" << interlockPattern
                   << "interlockPressPattern" << interlockPressPattern;
    }

    /*
     * ส่วนคำนวณเดิมยังทำงานเหมือนเดิม
     * ใช้ dist/volt local จากรอบนี้
     */
    if (phase == "A") {
        if (patternSelected) {
            calFLF(dist, volt, "A");
        }

        RecalculateWithMarginManual(dist, volt, "A");
    }
    else if (phase == "B") {
        if (patternSelected) {
            calFLF(dist, volt, "B");
        }

        RecalculateWithMarginManual(dist, volt, "B");
    }
    else if (phase == "C") {
        if (patternSelected) {
            calFLF(dist, volt, "C");
        }

        RecalculateWithMarginManual(dist, volt, "C");
    }

    return true;
}

void PLCServer::patternTimerFunction() {
    qDebug() << "patternTimerFunction:";
    if (numOfPattern >= maxNumOfPattern) {
        qDebug() << "patternTimer->stop() and clear all data voltageListA: จบที่ " << numOfPattern << " ครั้ง";
        interlockPattern = false;
        QJsonDocument jsonDoc;
        QJsonObject Param;
        QJsonObject().swap(Param);
        Param.insert("objectName", "PatternCount");
        Param.insert("msg", "PATTERN COUNT " + QString::number(numOfPattern));
        QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_data, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_data);

        QJsonObject().swap(Param);
        Param.insert("objectName", "PatternCount");
        Param.insert("msg", "PATTERN COUNT SUCCESS");
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient->state() == QAbstractSocket::ConnectedState)
                emit sendMessage(raw_data, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        emit sendToVNC(raw_data);

//        QJsonObject().swap(Param);
//        Param.insert("objectName", "Pop-up");
//        Param.insert("msg", "disable");
//        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
//        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
//            if (pClient->state() == QAbstractSocket::ConnectedState)
//                emit sendMessage(raw_data, pClient);
//            else
//                qDebug() << "Monitor_address:" << pClient->state();
//        }

        patternTimer->stop();
        return;
    }
    qDebug() << "ELSE patternTimerFunction:" << numOfPattern;
    QString datetime = getChrrentDateTime();
    QJsonDocument jsonDoc;
    QJsonObject Param;
    Param.insert("objectName", "PatternTest");
    QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    emit sendMessage(raw_data, FPGA_address);

    QJsonObject().swap(Param);
    Param.insert("objectName", "PatternCount");
    Param.insert("msg", "PATTERN COUNT " + QString::number(numOfPattern));
    raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    Q_FOREACH (QWebSocket *pClient, Monitor_address) {
        if (pClient->state() == QAbstractSocket::ConnectedState)
            emit sendMessage(raw_data, pClient);
        else
            qDebug() << "Monitor_address:" << pClient->state();
    }
    emit sendToVNC(raw_data);
    numOfPattern++;
    qDebug() << "numOfPattern" << numOfPattern << " maxNumOfPattern:" << maxNumOfPattern;
}

void PLCServer::updateDatePeriodic(bool monday, bool tuesday, bool wednesday, bool thursday, bool friday, bool saturday, bool sunday) {
    //    Monday, Tuesday, Wednesday, Thursday, Friday, Saturday and Sunday

    qDebug() << "updateDatePeriodic:DEBUG DAY Monday:" << monday << " Tuesday:" << tuesday << " Wednesday" << wednesday << " Thursday:" << thursday << " Friday:" << friday << " Saturday:" << saturday << " Sunday:" << sunday;

    emit preiodicSetting();
    //    if(monday != day->Monday){
    day->Monday = myDatabase->Monday;
    //    }
    //    if(tuesday != day->Tuesday){
    day->Tuesday = myDatabase->Tuesday;
    //    }
    //    if(wednesday != day->Wednesday){
    day->Wednesday = myDatabase->Wednesday;
    //    }
    //    if(thursday != day->Thursday){
    day->Thursday = myDatabase->Thursday;
    //    }
    //    if(friday != day->Friday){
    day->Friday = myDatabase->Friday;
    //    }
    //    if(saturday != day->Saturday){
    day->Saturday = myDatabase->Saturday;
    //    }
    //    if(sunday != day->Sunday){
    day->Sunday = myDatabase->Sunday;
    //    }
    qDebug() << " updateDatePeriodic:DEBUG";
    day->printinfo();
    lastStateday->printinfo();

    //    QJsonDocument jsonDoc;
    //    QJsonObject Param;
    //    Param.insert("objectName","dateRemote");
    //    Param.insert("Monday",monday);
    //    Param.insert("Tuesday",tuesday);
    //    Param.insert("Wednesday",wednesday);
    //    Param.insert("Thursday",thursday);
    //    Param.insert("Friday",friday);
    //    Param.insert("Saturday",saturday);
    //    Param.insert("Sunday",sunday);
    //    QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    //    Q_FOREACH (QWebSocket *pClient, Monitor_address)
    //    {
    //        if(pClient->state() == QAbstractSocket::ConnectedState)
    //            emit sendMessage(raw_data, pClient);
    //        else
    //            qDebug() << "Monitor_address:" << pClient->state();
    //    }
}

void PLCServer::updateTimePeriodic(QString t) {
    // qWarning() << "updateTimePeriodic::DEBUG t:" << t;
    emit preiodicSetting();
    day->times = myDatabase->times;
    QJsonDocument jsonDoc;
    QJsonObject Param;
    Param.insert("objectName", "updateTimeRemote");
    Param.insert("Time", day->times);
    QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    Q_FOREACH (QWebSocket *pClient, Monitor_address) {
        if (pClient->state() == QAbstractSocket::ConnectedState)
            emit sendMessage(raw_data, pClient);
        else
            qDebug() << "Monitor_address:" << pClient->state();
    }
    // qWarning() << "updateTimePeriodic ---> "<< raw_data;
    emit sendToVNC(raw_data);
}

QString PLCServer::getChrrentDateTime() {
    // Get current system time in nanoseconds
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();

    // Extract seconds and nanoseconds
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration) - seconds;

    // Convert to time_t for formatting
    std::time_t time_now = seconds.count();
    std::tm tm_now = *std::localtime(&time_now);

    // Use stringstream for formatted output
    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%d/%m/%Y %H:%M:%S") << "." << std::setw(9) << std::setfill('0') << nanoseconds.count();

    return QString::fromStdString(oss.str());
}

void PLCServer::selectUserFromWeb() {
    // qWarning() << "initMaster DEBUG";
    QJsonDocument jsonDoc;
    QJsonObject Param;
    Param.insert("objectName", "selectUserFromWeb");
    if (masterLFL == "MASTER") {
        Param.insert("userType", "SLAVE");
    }
    if (masterLFL == "SLAVE") {
        Param.insert("userType", "MASTER");
    }
    if(masterLFL == "STANDALONE"){
        Param.insert("userType", "STANDALONE");
        standAlone = true;
    }
    //    Param.insert("userType",masterLFL);
    Param.insert("ip_master", masterIP);
    Param.insert("ip_slave", slaveIP);
    QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    emit sendToSocketPLC(raw_data);
}

void PLCServer::initMaster() {
    // standAlone = false;
    // qWarning() << "initMaster DEBUG" << masterLFL;
    QJsonDocument jsonDoc;
    QJsonObject Param;
    Param.insert("objectName", "selectUser");
    Param.insert("userType", masterLFL);
    Param.insert("ip_master", masterIP);
    Param.insert("ip_slave", slaveIP);
    if (masterLFL == "MASTER") {
        Param.insert("RemoteTOMonitor", "REMOTE TO SLAVE");
    } else if (masterLFL == "SLAVE") {
        Param.insert("RemoteTOMonitor", "REMOTE TO MASTER");
    } else{
        Param.insert("RemoteTOMonitor", "STANDALONE");
    }

    QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();

    // qWarning() << "FUNCTION initMaster:" << raw_data;

    Q_FOREACH (QWebSocket *pClient, Monitor_address) {
        if (pClient->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(raw_data, pClient);
            qDebug() << "sendtomonitors";
        } else
            qDebug() << "Monitor_address:" << pClient->state();
    }
    emit sendToVNC(raw_data);

    Q_FOREACH (QWebSocket *pClient, webapp_address) {
        if (pClient->state() == QAbstractSocket::ConnectedState)
            emit sendMessage(raw_data, pClient);
        else
            qDebug() << "Monitor_address:" << pClient->state();
    }

    if (INPUT_PLC_address->state() == QAbstractSocket::ConnectedState) {
        emit sendMessage(raw_data, INPUT_PLC_address);
        qDebug() << "INPUT_PLC_address";
    } else
        qDebug() << "INPUT_PLC_address:" << INPUT_PLC_address->state();
}

void PLCServer::uploadToSNMP(QString name, QString url) {
    FullName = name;
    csvUrl = url;
}

void PLCServer::sendEmailParamToWeb() {
    qDebug() << "sendEmailParamToWeb:client";
    QJsonDocument jsonDoc;
    QJsonObject Param;
    QString raw_data;
    Param.insert("objectName", "SNMP_EMAIL_ENABLE");
    Param.insert("PLC_DO_ACTIVE_MAIL", Email_Param->PLC_DO_ERROR_MAIL);
    Param.insert("PLC_DI_ACTIVE_MAIL", Email_Param->PLC_DI_ERROR_MAIL);
    Param.insert("MODULE_HI_SPEED_PHASE_A_ERROR_MAIL", Email_Param->MODULE_HI_SPEED_PHASE_A_ERROR_MAIL);
    Param.insert("MODULE_HI_SPEED_PHASE_B_ERROR_MAIL", Email_Param->MODULE_HI_SPEED_PHASE_B_ERROR_MAIL);
    Param.insert("MODULE_HI_SPEED_PHASE_C_ERROR_MAIL", Email_Param->MODULE_HI_SPEED_PHASE_C_ERROR_MAIL);
    Param.insert("INTERNAL_PHASE_A_ERROR_MAIL", Email_Param->INTERNAL_PHASE_A_ERROR_MAIL);
    Param.insert("INTERNAL_PHASE_B_ERROR_MAIL", Email_Param->INTERNAL_PHASE_B_ERROR_MAIL);
    Param.insert("INTERNAL_PHASE_C_ERROR_MAIL", Email_Param->INTERNAL_PHASE_C_ERROR_MAIL);
    Param.insert("GPS_MODULE_FAIL_MAIL", Email_Param->GPS_MODULE_FAIL_MAIL);
    Param.insert("SYSTEM_INITIAL_MAIL", Email_Param->SYSTEM_INITIAL_MAIL);
    Param.insert("COMMUNICATION_ERROR_MAIL", Email_Param->COMMUNICATION_ERROR_MAIL);
    Param.insert("RELAY_START_EVENT_MAIL", Email_Param->RELAY_START_EVENT_MAIL);
    Param.insert("SURGE_START_EVENT_MAIL", Email_Param->SURGE_START_EVENT_MAIL);
    Param.insert("PERIODIC_TEST_EVENT_MAIL", Email_Param->PERIODIC_TEST_EVENT_MAIL);
    Param.insert("MANUAL_TEST_EVENT_MAIL", Email_Param->MANUAL_TEST_EVENT_MAIL);
    Param.insert("LFL_FAIL_MAIL", Email_Param->LFL_FAIL_MAIL);
    Param.insert("LFL_OPERATE_MAIL", Email_Param->LFL_OPERATE_MAIL);
    //    Param.insert("DELAY_EVENT_MAIL",Email_Param->DELAY_EVENT_MAIL);
    //     Param.insert("DELAY_ALARM_MAIL",Email_Param->DELAY_ALARM_MAIL);
    jsonDoc.setObject(Param);
    raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    Q_FOREACH (QWebSocket *pClient, webapp_address) {
        if (pClient->state() == QAbstractSocket::ConnectedState)
            emit sendMessage(raw_data, pClient);
        else
            qDebug() << "Monitor_address:" << pClient->state();
    }
    emit sendToVNC(raw_data);
}

void PLCServer::sendEmailParamToSNMP() {
    qDebug() << "sendEmailParamToSNMP DEBUG";
    QJsonDocument jsonDoc;
    QJsonObject Param;
    QString raw_data;
    Param.insert("objectName", "SNMP_EMAIL_ENABLE");
    Param.insert("PLC_DO_ACTIVE_MAIL", Email_Param->PLC_DO_ERROR_MAIL);
    Param.insert("PLC_DI_ACTIVE_MAIL", Email_Param->PLC_DI_ERROR_MAIL);
    Param.insert("MODULE_HI_SPEED_PHASE_A_ERROR_MAIL", Email_Param->MODULE_HI_SPEED_PHASE_A_ERROR_MAIL);
    Param.insert("MODULE_HI_SPEED_PHASE_B_ERROR_MAIL", Email_Param->MODULE_HI_SPEED_PHASE_B_ERROR_MAIL);
    Param.insert("MODULE_HI_SPEED_PHASE_C_ERROR_MAIL", Email_Param->MODULE_HI_SPEED_PHASE_C_ERROR_MAIL);
    Param.insert("INTERNAL_PHASE_A_ERROR_MAIL", Email_Param->INTERNAL_PHASE_A_ERROR_MAIL);
    Param.insert("INTERNAL_PHASE_B_ERROR_MAIL", Email_Param->INTERNAL_PHASE_B_ERROR_MAIL);
    Param.insert("INTERNAL_PHASE_C_ERROR_MAIL", Email_Param->INTERNAL_PHASE_C_ERROR_MAIL);
    Param.insert("GPS_MODULE_FAIL_MAIL", Email_Param->GPS_MODULE_FAIL_MAIL);
    Param.insert("SYSTEM_INITIAL_MAIL", Email_Param->SYSTEM_INITIAL_MAIL);
    Param.insert("COMMUNICATION_ERROR_MAIL", Email_Param->COMMUNICATION_ERROR_MAIL);
    Param.insert("RELAY_START_EVENT_MAIL", Email_Param->RELAY_START_EVENT_MAIL);
    Param.insert("SURGE_START_EVENT_MAIL", Email_Param->SURGE_START_EVENT_MAIL);
    Param.insert("PERIODIC_TEST_EVENT_MAIL", Email_Param->PERIODIC_TEST_EVENT_MAIL);
    Param.insert("MANUAL_TEST_EVENT_MAIL", Email_Param->MANUAL_TEST_EVENT_MAIL);
    Param.insert("LFL_FAIL_MAIL", Email_Param->LFL_FAIL_MAIL);
    Param.insert("LFL_OPERATE_MAIL", Email_Param->LFL_OPERATE_MAIL);
    Param.insert("DELAY_EVENT_MAIL", Email_Param->DELAY_EVENT_MAIL);
    Param.insert("DELAY_ALARM_MAIL", Email_Param->DELAY_ALARM_MAIL);
    jsonDoc.setObject(Param);
    raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    if (snmp_address->state() == QAbstractSocket::ConnectedState)
        emit sendMessage(raw_data, snmp_address);
    else
        qDebug() << "snmp_address:" << snmp_address->state();
}

void PLCServer::getSettingNetworks(QString ip, QString gateway, QString snmp, QString ntp, bool PlcDoError, bool PlcDiError, bool ModuleHispeedPhaseAError, bool ModuleHispeedPhaseBError, bool ModuleHispeedPhaseCError, bool modbusPhaseAError, bool modbusPhaseBError, bool modbusPhaseCError, bool GpsModuleFail, bool SystemInital, bool CommunicationError, bool RelayStartEvent, bool surgeStartEvent, bool PeriodicStartEvent, bool ManualTestEvent, bool LFLFail, bool LFLOperate) {
    //    qDebug() << "Fetched settings:";
    //    qDebug() << "IP:" << ip << "Gateway:" << gateway << "SNMP:" << snmp << "NTP:" << ntp;
    //    qDebug() << "PLC Errors:" << PlcDoError << PlcDiError;
    //    qDebug() << "Module Errors:" << ModuleHispeedPhaseAError << ModuleHispeedPhaseBError << ModuleHispeedPhaseCError;
    //    qDebug() << "Modbus Errors:" << modbusPhaseAError << modbusPhaseBError << modbusPhaseCError;
    //    qDebug() << "GPS Fail:" << GpsModuleFail << "System Init:" << SystemInital;
    //    qDebug() << "Comm Error:" << CommunicationError << "Relay Event:" << RelayStartEvent;
    //    qDebug() << "Surge Event:" << surgeStartEvent << "Periodic Event:" << PeriodicStartEvent;
    //    qDebug() << "Manual Test Event:" << ManualTestEvent << "LFL Fail:" << LFLFail << "LFL Operate:" << LFLOperate;
    networks->ip_address = ip;
    networks->ip_gateway = gateway;
    networks->ip_snmp = snmp;
    networks->ip_timeserver = ntp;
    snmp_param->PLC_DO_ERROR = PlcDoError;
    snmp_param->PLC_DI_ERROR = PlcDiError;
    snmp_param->MODULE_HI_SPEED_PHASE_A_ERROR = ModuleHispeedPhaseAError;
    snmp_param->MODULE_HI_SPEED_PHASE_B_ERROR = ModuleHispeedPhaseBError;
    snmp_param->MODULE_HI_SPEED_PHASE_C_ERROR = ModuleHispeedPhaseCError;
    snmp_param->INTERNAL_PHASE_A_ERROR = modbusPhaseAError;
    snmp_param->INTERNAL_PHASE_B_ERROR = modbusPhaseBError;
    snmp_param->INTERNAL_PHASE_C_ERROR = modbusPhaseCError;
    snmp_param->GPS_MODULE_FAIL = GpsModuleFail;
    snmp_param->SYSTEM_INITIAL = SystemInital;
    snmp_param->COMMUNICATION_ERROR = CommunicationError;
    snmp_param->RELAY_START_EVENT = RelayStartEvent;
    snmp_param->SURGE_START_EVENT = surgeStartEvent;
    snmp_param->PERIODIC_TEST_EVENT = PeriodicStartEvent;
    snmp_param->MANUAL_TEST_EVENT = ManualTestEvent;
    snmp_param->LFL_FAIL = LFLFail;
    snmp_param->LFL_OPERATE = LFLOperate;
}

void PLCServer::sendFTPParam() {
    qDebug() << "sendFTPParam DEBUG";
    QJsonDocument jsonDoc;
    QJsonObject Param;
    QString raw_data;
    Param.insert("objectName", "UpdateFTPParameter");  // Name
    Param.insert("ftpIP", FTP_Param_->FTP_IP);
    Param.insert("username", FTP_Param_->USERNAME);
    Param.insert("password", FTP_Param_->PASSWORD);
    Param.insert("periodic_file", FTP_Param_->PERIODIC_FILE);
    Param.insert("relay_file", FTP_Param_->RELAY_FILE);
    Param.insert("surge_file", FTP_Param_->SURGE_FILE);
    Param.insert("manual_file", FTP_Param_->MANUAL_FILE);
    Param.insert("pattern_file", FTP_Param_->PATTERN_FILE);
    jsonDoc.setObject(Param);
    raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    Q_FOREACH (QWebSocket *pClient, webapp_address) {
        if (pClient->state() == QAbstractSocket::ConnectedState)
            emit sendMessage(raw_data, pClient);
        else
            qDebug() << "webapp_address:" << pClient->state();
    }
}

void PLCServer::loopNetwork() {
    qDebug() << "loopNetwork networksTemp != networks";
    if (networksTemp != networks) {
        qDebug() << "networksTemp != networks";
        updateNetwork(networks->dhcpmethod.toInt(), networks->ip_address, networks->subnet, networks->ip_gateway, networks->pridns, networks->secdns, "eth0");
        // myDatabase->updateSettingNetwork(networks->ip_address, networks->ip_gateway, networks->ip_snmp, networks->ip_timeserver, snmp_param->PLC_DO_ERROR, snmp_param->PLC_DI_ERROR, snmp_param->MODULE_HI_SPEED_PHASE_A_ERROR, snmp_param->MODULE_HI_SPEED_PHASE_B_ERROR, snmp_param->MODULE_HI_SPEED_PHASE_C_ERROR, snmp_param->INTERNAL_PHASE_A_ERROR, snmp_param->INTERNAL_PHASE_B_ERROR, snmp_param->INTERNAL_PHASE_C_ERROR, snmp_param->GPS_MODULE_FAIL, snmp_param->SYSTEM_INITIAL, snmp_param->COMMUNICATION_ERROR, snmp_param->RELAY_START_EVENT, snmp_param->SURGE_START_EVENT, snmp_param->PERIODIC_TEST_EVENT, snmp_param->MANUAL_TEST_EVENT, snmp_param->LFL_FAIL, snmp_param->LFL_OPERATE);
        *networksTemp = *networks;
        reset_ip = false;
    }

    myDatabase->updateSettingNetwork(networks->ip_address, networks->ip_gateway, networks->ip_snmp, networks->ip_timeserver, snmp_param->PLC_DO_ERROR, snmp_param->PLC_DI_ERROR, snmp_param->MODULE_HI_SPEED_PHASE_A_ERROR, snmp_param->MODULE_HI_SPEED_PHASE_B_ERROR, snmp_param->MODULE_HI_SPEED_PHASE_C_ERROR, snmp_param->INTERNAL_PHASE_A_ERROR, snmp_param->INTERNAL_PHASE_B_ERROR, snmp_param->INTERNAL_PHASE_C_ERROR, snmp_param->GPS_MODULE_FAIL, snmp_param->SYSTEM_INITIAL, snmp_param->COMMUNICATION_ERROR, snmp_param->RELAY_START_EVENT, snmp_param->SURGE_START_EVENT, snmp_param->PERIODIC_TEST_EVENT, snmp_param->MANUAL_TEST_EVENT, snmp_param->LFL_FAIL, snmp_param->LFL_OPERATE);
    loopNetworkTimer->stop();
}

void PLCServer::selectProgram(int num)
{
    qDebug() << "begin stop all service";
    qDebug() << "stop all service";

    if (num == 0) {
        QThread::msleep(50);
        runProcessChecked(QStringLiteral("sh"), QStringList{QStringLiteral("/usr/local/bin/callinput.sh")});
        qDebug() << "start INPUT_GPIO service";
    } else if (num == 1) {
        QThread::msleep(50);
        runProcessChecked(QStringLiteral("sh"), QStringList{QStringLiteral("/usr/local/bin/callplc.sh")});
        qDebug() << "start openplc service";
    }
}

void PLCServer::getReadyFolder()
{
    runProcessChecked(QStringLiteral("sh"), QStringList{QStringLiteral("/usr/local/bin/mount_sdcard.sh")}, 30000);

    ensureRequiredPath(QStringLiteral("PIC_PATH"), PIC_PATH);
    ensureRequiredPath(QStringLiteral("MANUAL_PATH"), MANUAL_PATH);
    ensureRequiredPath(QStringLiteral("RELAY_PATH"), RELAY_PATH);
    ensureRequiredPath(QStringLiteral("SURGE_PATH"), SURGE_PATH);
    ensureRequiredPath(QStringLiteral("PATTERN_PATH"), PATTERN_PATH);
    ensureRequiredPath(QStringLiteral("PERIODIC_PATH"), PERIODIC_PATH);
    ensureRequiredPath(QStringLiteral("TOWER_NO"), TOWER_NO);
}

void PLCServer::checkFolderExist()
{
    QDir dir(QStringLiteral("/mnt/sdcard/event_records"));
    if (dir.exists()) {
        qDebug() << "/mnt/sdcard/event_records Directory exists!";
    } else {
        PIC_PATH = QStringLiteral("/home/pi/Save_ScreenPicture");
        EVENT_PATH = QStringLiteral("/home/pi/event_records/");
        TOWER_NO = QStringLiteral("/home/pi/event_records/tower/");
        ensureRequiredPath(QStringLiteral("TOWER_NO"), TOWER_NO);
        emit updatePathNotMount(TOWER_NO, EVENT_PATH);
        qDebug() << "event_records Directory does not exist!" << EVENT_PATH << PIC_PATH << "/home/pi/Save_ScreenPicture";
    }
    qDebug() << "EVENT_PATHll" << EVENT_PATH;
}

void PLCServer::sendToWeb() {
    qDebug() << "sendToWeb refresh";
    QJsonDocument jsonDoc;
    QJsonObject Param;
    QString raw_data;
    Param.insert("menuID", "refresh");
    jsonDoc.setObject(Param);
    raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    Q_FOREACH (QWebSocket *pClient, webapp_address) {
        if (pClient->state() == QAbstractSocket::ConnectedState)
            emit sendMessage(raw_data, pClient);
        else
            qDebug() << "Monitor_address:" << pClient->state();
    }
}

void PLCServer::assignPATHCSV(QString name, QString fullpath) {
    // assignPATHCSV is now a storage/verification step only.  sendMail must be
    // emitted only after both event CSV + picture are physically verified and
    // picture FTP upload has succeeded.
    ensureEventAuditContext(modeName, DateKept, TimeKept, QStringLiteral("EVENT"), name);

    QJsonObject auditDetail;
    auditDetail.insert(QStringLiteral("reportedName"), name);
    auditDetail.insert(QStringLiteral("reportedPath"), fullpath);

    const QString verifiedPath = resolveExistingEventCsvPath(fullpath,
                                                              name,
                                                              QString(),
                                                              QString(),
                                                              QString());
    if (verifiedPath.isEmpty()) {
        qWarning() << "[PATH-CHECK][CSV] assignPATHCSV rejected unverified/non-event path:"
                   << "name=" << name << "path=" << fullpath;
        auditDetail.insert(QStringLiteral("verified"), false);
        auditEventStep(QStringLiteral("CSV_LOCAL_READY"),
                       QStringLiteral("Event CSV created and physically available"),
                       QStringLiteral("FAIL"),
                       QStringLiteral("Database reported an Event CSV path but filesystem/type verification rejected it; Pattern cannot substitute Event CSV"),
                       fullpath,
                       QStringLiteral("verified Event CSV context"),
                       auditDetail);
        return;
    }

    fullpathCSV = verifiedPath;
    fullnameCSV = QFileInfo(verifiedPath).fileName();

    const QFileInfo csvInfo(fullpathCSV);
    auditDetail.insert(QStringLiteral("verified"), true);
    auditDetail.insert(QStringLiteral("verifiedPath"), fullpathCSV);
    auditDetail.insert(QStringLiteral("verifiedName"), fullnameCSV);
    auditDetail.insert(QStringLiteral("size"), double(csvInfo.size()));
    auditDetail.insert(QStringLiteral("lastModified"), csvInfo.lastModified().toString(Qt::ISODateWithMs));
    auditEventStep(QStringLiteral("CSV_LOCAL_READY"),
                   QStringLiteral("Event CSV created and physically available"),
                   QStringLiteral("PASS"),
                   QStringLiteral("Event CSV exists as a physical non-Pattern file and is stored as the current verified CSV context"),
                   fullpath,
                   fullpathCSV,
                   auditDetail);

    qDebug() << "[PATH-CHECK][CSV] assignPATHCSV stored VERIFIED event path:"
             << "CSVPATH=" << fullpathCSV
             << "CSVname=" << fullnameCSV
             << "size=" << QFileInfo(fullpathCSV).size();
}

void PLCServer::startFtpTimer(QString cmd1, QString cmd2, QString cmd3)
{
    qDebug() << "startFtpTimer"
             << redactedCommand(cmd1)
             << redactedCommand(cmd2)
             << redactedCommand(cmd3);
    ftpUrlFolder = cmd1;
    ftpUrlFolder2 = cmd2;
    ftpUrlFolder3 = cmd3;
    stopThread2 = false;
    int ret = pthread_create(&idThread2, NULL, ThreadFunc2, this);
    if (ret == 0) {
        qDebug() << ("Thread2 created successfully.\n");
    } else {
        qDebug() << ("Thread2 not created.\n");
    }
}

void PLCServer::lftpSendFunction()
{
    int n = 0;
    qDebug() << "Thread2 loopFtpTimerFunction RUNNING" << stopThread2;
    while (!stopThread2) {
        if (n >= 20) {
            break;
        }
        qDebug() << "Thread2 loopFtpTimerFunction RUNNING";
        n++;
        QThread::msleep(50);
    }
    while (!stopThread3) {
        qDebug() << "Thread3 lftpSendFunction" << redactedCommand(uploadsFTP);

        if (!runFtpCommandNow(uploadsFTP, 8000)) {
            qWarning() << "[FTP] uploadFTP skipped/failed:" << redactedCommand(uploadsFTP);
        }

        QThread::msleep(20);
        stopThread3 = true;
    }
    qDebug() << "Thread3 is dead.\n";
}

void PLCServer::loopFtpTimerFunction()
{
    qDebug() << "Thread2 loopFtpTimerFunction"
             << redactedCommand(ftpUrlFolder)
             << redactedCommand(ftpUrlFolder2)
             << " ftpUrlFolder3" << redactedCommand(ftpUrlFolder3);

    /*
     * cmd1 = สร้าง folder ชั้นหลัก
     * ถ้า FTP เข้าไม่ได้ หรือสร้างไม่ได้ ให้หยุดทันที
     * ไม่ต้องยิง cmd2/cmd3 ต่อ เพราะจะ block ระบบซ้ำ
     */
    if (!runFtpCommandNow(ftpUrlFolder, 8000)) {
        qWarning() << "[FTP] stop folder/upload chain at cmd1";
        stopThread2 = true;
        qDebug() << "Thread2 is dead.\n";
        return;
    }

    QThread::msleep(20);

    if (!runFtpCommandNow(ftpUrlFolder2, 8000)) {
        qWarning() << "[FTP] stop folder/upload chain at cmd2";
        stopThread2 = true;
        qDebug() << "Thread2 is dead.\n";
        return;
    }

    QThread::msleep(20);

    if (!runFtpCommandNow(ftpUrlFolder3, 8000)) {
        qWarning() << "[FTP] upload command failed at cmd3";
    }

    stopThread2 = true;
    qDebug() << "Thread2 is dead.\n";
}

void PLCServer::updatePATHEmail(QString pattern,QString pic){
    fullPathPattern = pattern;
    fullPicPATH = pic;
    // QJsonDocument jsonDoc;
    // QJsonObject Param;
    // QString raw_datas;
    // Param.insert("objectName", "sendMail");
    // Param.insert("CSVPATH", fullnameCSV);
    // Param.insert("CSVname", fullnameCSV);
    // Param.insert("PicPATH", fullpathCSV);
    // Param.insert("Picname", fullnameCSV);
    // Param.insert("CSVPatternPATH", myDatabase -> selectPatterPath);
    // Param.insert("CSVPatternname", myDatabase -> selectPatterName);
    // jsonDoc.setObject(Param);
    // raw_datas = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    // qDebug() << "sendMail:" << raw_datas;
    // emit sendToMonitor(raw_datas);
    // emit sendToVNC(raw_datas);
    // qWarning() << "updatePATHEmail fullPicPATH ->" << fullPicPATH << " fullPathPattern ->" << fullPathPattern;
}

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

bool PLCServer::buildVerifiedSyncPaths(QString *syncPicPayloadOut,
                                           QString *patternPath,
                                           QString *failureReason) const
{
    auto fail = [&](const QString &reason) -> bool {
        if (syncPicPayloadOut) {
            syncPicPayloadOut->clear();
        }
        if (patternPath) {
            patternPath->clear();
        }
        if (failureReason) {
            *failureReason = reason;
        }
        return false;
    };

    if (!syncPicPayloadOut || !patternPath) {
        return fail(QStringLiteral("SYNC output pointer is null"));
    }

    syncPicPayloadOut->clear();
    patternPath->clear();
    if (failureReason) {
        failureReason->clear();
    }

    // Hard gate: a SYNC event may only consume a bundle that completed the
    // Picture + Event CSV mandatory verification and verified FTP transaction.
    if (!verifiedEventBundleReady) {
        return fail(QStringLiteral(
            "current event has no verified Picture + Event CSV + FTP bundle"));
    }

    const QString eventMode = canonicalEventCategory(pictureEventModeSnapshot);
    if (eventMode != QStringLiteral("Manual") &&
        eventMode != QStringLiteral("Relay") &&
        eventMode != QStringLiteral("Surge") &&
        eventMode != QStringLiteral("Periodic")) {
        return fail(QStringLiteral("invalid verified event mode: %1")
                        .arg(pictureEventModeSnapshot));
    }

    // SYNC must use the authoritative eventRecord base folder, not Picture
    // capture time.  pictureEvent* is retained only as a legacy fallback.
    QString eventDate = eventBaseDateSnapshot.trimmed();
    QString eventTime = eventBaseTimeSnapshot.trimmed();
    if (!QDate::fromString(eventDate, QStringLiteral("yyyy-MM-dd")).isValid() ||
        !QTime::fromString(eventTime, QStringLiteral("HH-mm-ss")).isValid()) {
        eventDate = pictureEventDateSnapshot.trimmed();
        eventTime = pictureEventTimeSnapshot.trimmed();
    }
    if (!QDate::fromString(eventDate, QStringLiteral("yyyy-MM-dd")).isValid() ||
        !QTime::fromString(eventTime, QStringLiteral("HH-mm-ss")).isValid()) {
        return fail(QStringLiteral("invalid verified event date/time: %1 %2")
                        .arg(eventDate, eventTime));
    }

    if (verifiedPictureLocalPathSnapshot.trimmed().isEmpty() ||
        verifiedEventCsvLocalPathSnapshot.trimmed().isEmpty()) {
        return fail(QStringLiteral("mandatory verified local path snapshot is empty"));
    }

    // Re-verify the physical mandatory files at the actual SYNC publish point.
    // This is intentionally later than getScreenPictureandSave() and
    // loopWaitPicSlot(), so deletion/cleanup/race conditions are detected.
    const QString verifiedPicPath = resolveExistingPicturePath(
        verifiedPictureLocalPathSnapshot,
        QFileInfo(verifiedPictureLocalPathSnapshot).fileName(),
        eventDate,
        eventTime);

    const QString verifiedCsvPath = resolveExistingEventCsvPath(
        verifiedEventCsvLocalPathSnapshot,
        QFileInfo(verifiedEventCsvLocalPathSnapshot).fileName(),
        eventMode,
        eventDate,
        eventTime);

    if (verifiedPicPath.isEmpty()) {
        return fail(QStringLiteral(
            "mandatory Picture missing/invalid at SYNC publish: %1")
                        .arg(verifiedPictureLocalPathSnapshot));
    }

    if (verifiedCsvPath.isEmpty()) {
        return fail(QStringLiteral(
            "mandatory Event CSV missing/invalid/Pattern at SYNC publish: %1")
                        .arg(verifiedEventCsvLocalPathSnapshot));
    }

    // A resolver is useful before FTP, but after a transaction has been
    // verified it is NOT allowed to silently switch to another candidate.
    const QString expectedPic =
        QFileInfo(verifiedPictureLocalPathSnapshot).absoluteFilePath();
    const QString expectedCsv =
        QFileInfo(verifiedEventCsvLocalPathSnapshot).absoluteFilePath();

    if (QFileInfo(verifiedPicPath).absoluteFilePath() != expectedPic) {
        return fail(QStringLiteral(
            "Picture resolver changed physical file at SYNC publish"));
    }

    if (QFileInfo(verifiedCsvPath).absoluteFilePath() != expectedCsv) {
        return fail(QStringLiteral(
            "Event CSV resolver changed physical file at SYNC publish"));
    }

    // Physical regular/readable checks again. resolveExistingPicturePath() also
    // performs strict image validation; resolveExistingEventCsvPath() enforces
    // Event-mode ownership and Pattern exclusion.
    qint64 picSize = 0;
    qint64 csvSize = 0;
    if (!isVerifiedLocalFile(verifiedPicPath, &picSize) || picSize <= 0) {
        return fail(QStringLiteral(
            "Picture physical verification failed at SYNC publish: %1")
                        .arg(verifiedPicPath));
    }
    if (!isVerifiedLocalFile(verifiedCsvPath, &csvSize) || csvSize <= 0) {
        return fail(QStringLiteral(
            "Event CSV physical verification failed at SYNC publish: %1")
                        .arg(verifiedCsvPath));
    }

    QString eventRemotePath = verifiedFtpEventCsvRemotePath.trimmed();
    QString pictureRemotePath = verifiedFtpPictureRemotePath.trimmed();

    // Pattern has a different compatibility contract from Event CSV/Picture:
    // publish the exact verified source path on this PLC filesystem.  The FTP
    // Pattern copy may still exist for backup/transfer, but it is not used as
    // SYNC.PATTERN or top-level PatternPATH.
    QString patternLocalPath = verifiedPatternLocalPathSnapshot.trimmed(); // optional

    eventRemotePath.replace('\\', '/');
    pictureRemotePath.replace('\\', '/');
    patternLocalPath.replace('\\', '/');

    if (eventRemotePath.isEmpty() || pictureRemotePath.isEmpty()) {
        return fail(QStringLiteral(
            "verified FTP mandatory remote path is empty (csv=%1 pic=%2)")
                        .arg(eventRemotePath, pictureRemotePath));
    }

    const QString eventRemoteName = QFileInfo(eventRemotePath).fileName();
    const QString pictureRemoteName = QFileInfo(pictureRemotePath).fileName();
    if (eventRemoteName.isEmpty() || pictureRemoteName.isEmpty()) {
        return fail(QStringLiteral("verified FTP mandatory remote filename is empty"));
    }

    // Contract #1 + #2: Event CSV can never be a Pattern candidate.
    if (eventRemoteName.contains(QStringLiteral("_PATTERN"), Qt::CaseInsensitive) ||
        eventRemotePath.contains(QStringLiteral("/Pattern/"), Qt::CaseInsensitive) ||
        eventRemotePath.endsWith(QStringLiteral("/Pattern"), Qt::CaseInsensitive)) {
        return fail(QStringLiteral(
            "FTP Event CSV violates Pattern exclusion: %1")
                        .arg(eventRemotePath));
    }

    // Both remote mandatory files must belong to the same exact event folder.
    // We intentionally verify the mode/date/time segment as well as filenames.
    const QString eventFolderToken =
        QStringLiteral("/%1/%2/%3/").arg(eventMode, eventDate, eventTime);
    if (!eventRemotePath.contains(eventFolderToken, Qt::CaseInsensitive) ||
        !pictureRemotePath.contains(eventFolderToken, Qt::CaseInsensitive)) {
        return fail(QStringLiteral(
            "FTP mandatory paths do not belong to verified event folder %1")
                        .arg(eventFolderToken));
    }

    const QString localCsvName = QFileInfo(verifiedCsvPath).fileName();
    const QString expectedRemoteCsvName =
        canonicalRemoteEventCsvName(localCsvName, eventDate, eventTime);
    const QString localPicName = QFileInfo(verifiedPicPath).fileName();

    if (eventRemoteName.compare(expectedRemoteCsvName, Qt::CaseInsensitive) != 0) {
        return fail(QStringLiteral(
            "FTP Event CSV canonical name mismatch: remote=%1 expected=%2 local=%3")
                        .arg(eventRemoteName, expectedRemoteCsvName, localCsvName));
    }

    if (pictureRemoteName.compare(localPicName, Qt::CaseInsensitive) != 0) {
        return fail(QStringLiteral(
            "FTP Picture name mismatch: remote=%1 local=%2")
                        .arg(pictureRemoteName, localPicName));
    }

    // The Picture component of the legacy SYNC.PicPATH payload must identify
    // the exact verified remote Picture FILE.  Directory-only values are never
    // allowed even though PicPATH itself will later be wrapped in the composite
    // "EVENT ... | PICTURE ... | PATTERN ..." compatibility string.
    if (pictureRemotePath.endsWith(QLatin1Char('/')) ||
        pictureRemoteName.isEmpty() ||
        pictureRemoteName == QStringLiteral(".") ||
        pictureRemoteName == QStringLiteral("..")) {
        return fail(QStringLiteral(
            "FTP Picture remote path is not a file path: %1")
                        .arg(pictureRemotePath));
    }

    // A valid mandatory Event CSV remote path must also identify the concrete
    // uploaded file (this is verified even though Event CSV is not exposed as a
    // separate legacy SYNC field).
    if (eventRemotePath.endsWith(QLatin1Char('/')) ||
        eventRemoteName.isEmpty() ||
        eventRemoteName == QStringLiteral(".") ||
        eventRemoteName == QStringLiteral("..")) {
        return fail(QStringLiteral(
            "FTP Event CSV remote path is not a file path: %1")
                        .arg(eventRemotePath));
    }

    // Pattern is optional, but when present it must still be a real readable
    // file inside PATTERN_PATH on this PLC.  If it disappeared after the final
    // event gate, publish an empty Pattern instead of substituting any FTP/Event
    // path.  This re-check happens at the actual SYNC publish point.
    qint64 patternSize = 0;
    if (!patternLocalPath.isEmpty()) {
        const QString patternBase = QDir(PATTERN_PATH).absolutePath();
        const QString patternAbsolute = QFileInfo(patternLocalPath).absoluteFilePath();
        QString patternBasePrefix = patternBase;
        if (!patternBasePrefix.endsWith(QLatin1Char('/'))) {
            patternBasePrefix += QLatin1Char('/');
        }

        const bool insidePatternRoot =
            patternAbsolute == patternBase ||
            patternAbsolute.startsWith(patternBasePrefix);

        const QString patternLocalName = QFileInfo(patternAbsolute).fileName();
        if (!insidePatternRoot ||
            patternAbsolute.endsWith(QLatin1Char('/')) ||
            patternLocalName.isEmpty() ||
            patternLocalName == QStringLiteral(".") ||
            patternLocalName == QStringLiteral("..") ||
            !isVerifiedLocalFile(patternAbsolute, &patternSize) ||
            patternSize <= 0) {
            qWarning() << "[SYNC-VERIFY][PATTERN] discard invalid optional local Pattern path:"
                       << patternLocalPath;
            patternLocalPath.clear();
            patternSize = 0;
        } else {
            patternLocalPath = QDir::cleanPath(patternAbsolute);
        }
    }

    // IMPORTANT LEGACY/COMPATIBILITY CONTRACT:
    //
    // SYNC.PicPATH remains the existing composite payload consumed by the
    // downstream SNMP/monitor logic, but Pattern intentionally points to its
    // verified source file on the PLC filesystem:
    //
    //   EVENT <verified Event CSV remote FTP file>
    //   | PICTURE <verified Picture remote FTP file>
    //   | PATTERN <verified local Pattern filesystem file or empty>
    //
    // Event CSV/Picture must still come from the verified FTP transaction.
    // Pattern must come from verifiedPatternLocalPathSnapshot; never read live
    // mutable selectPatterPath here because another event may have changed it.
    const QString syncPicPayload =
        QStringLiteral("EVENT ") + eventRemotePath +
        QStringLiteral(" | PICTURE ") + pictureRemotePath +
        QStringLiteral(" | PATTERN ") + patternLocalPath;

    *syncPicPayloadOut = syncPicPayload;
    *patternPath = patternLocalPath;

    qDebug() << "[SYNC-VERIFY][PASS]"
             << "mode=" << eventMode
             << "date=" << eventDate
             << "time=" << eventTime
             << "csvLocal=" << verifiedCsvPath
             << "csvSize=" << csvSize
             << "picLocal=" << verifiedPicPath
             << "picSize=" << picSize
             << "eventRemote=" << eventRemotePath
             << "eventCsvLocalName=" << localCsvName
             << "eventCsvExpectedRemoteName=" << expectedRemoteCsvName
             << "pictureRemote=" << pictureRemotePath
             << "PicPATHPayload=" << syncPicPayload
             << "patternLocal=" << (patternLocalPath.isEmpty()
                                           ? QStringLiteral("<optional-empty>")
                                           : patternLocalPath)
             << "patternSize=" << patternSize
             << "patternFtpBackup=" << (verifiedFtpPatternRemotePath.isEmpty()
                                               ? QStringLiteral("<optional-empty>")
                                               : verifiedFtpPatternRemotePath);

    return true;
}

void PLCServer::loopWaitPicSlot()
{
    QString syncPicPayload;
    QString syncPatternPath;
    QString failureReason;

    if (!buildVerifiedSyncPaths(&syncPicPayload,
                                &syncPatternPath,
                                &failureReason)) {
        qWarning() << "[loopWaitPicSlot][BLOCK]" << failureReason;
        QJsonObject d;
        d.insert(QStringLiteral("failureReason"), failureReason);
        d.insert(QStringLiteral("verifiedEventBundleReady"), verifiedEventBundleReady);
        d.insert(QStringLiteral("csvLocal"), verifiedEventCsvLocalPathSnapshot);
        d.insert(QStringLiteral("picLocal"), verifiedPictureLocalPathSnapshot);
        d.insert(QStringLiteral("ftpCsv"), verifiedFtpEventCsvRemotePath);
        d.insert(QStringLiteral("ftpPic"), verifiedFtpPictureRemotePath);
        auditEventStep(QStringLiteral("SYNC_VERIFY"),
                       QStringLiteral("Final SYNC verification"),
                       QStringLiteral("FAIL"),
                       QStringLiteral("Verified SYNC path builder rejected the event bundle"),
                       verifiedEventCsvLocalPathSnapshot + QStringLiteral(" | ") + verifiedPictureLocalPathSnapshot,
                       QStringLiteral("SYNC compatibility payload"), d);
        auditEventStep(QStringLiteral("LOOP_WAIT_PIC"),
                       QStringLiteral("Prepare verified legacy SYNC path payload"),
                       QStringLiteral("FAIL"),
                       QStringLiteral("Cannot prepare legacy SYNC PicPATH because final path verification failed"),
                       QStringLiteral("verified event bundle"), QStringLiteral("fullPicPATH/fullPathPattern"), d);
        fullPicPATH.clear();
        fullPathPattern.clear();
        fullstate = false;
        if (loopWaitPic) {
            loopWaitPic->stop();
        }
        return;
    }

    // Preserve the legacy composite SYNC format. Event CSV/Picture come from
    // verified FTP paths while Pattern, when present, is the verified local
    // source file on this PLC filesystem.
    // fullPicPATH is still compatibility state only and is never trusted as
    // an input to final verification.
    fullPicPATH = syncPicPayload;
    fullPathPattern = syncPatternPath; // optional; may legitimately be empty
    fullstate = true;

    {
        QJsonObject d;
        d.insert(QStringLiteral("PicPATHPayload"), fullPicPATH);
        d.insert(QStringLiteral("PatternPATH"), fullPathPattern);
        d.insert(QStringLiteral("patternOptional"), true);
        auditEventStep(QStringLiteral("SYNC_VERIFY"),
                       QStringLiteral("Final SYNC verification"),
                       QStringLiteral("PASS"),
                       QStringLiteral("Mandatory local and FTP paths passed SYNC path verification"),
                       verifiedEventCsvLocalPathSnapshot + QStringLiteral(" | ") + verifiedPictureLocalPathSnapshot,
                       syncPicPayload, d);
        auditEventStep(QStringLiteral("LOOP_WAIT_PIC"),
                       QStringLiteral("Prepare verified legacy SYNC path payload"),
                       QStringLiteral("PASS"),
                       QStringLiteral("Prepared legacy EVENT | PICTURE | PATTERN payload from verified FTP mandatory files and verified local Pattern source"),
                       verifiedFtpEventCsvRemotePath + QStringLiteral(" | ") + verifiedFtpPictureRemotePath +
                           QStringLiteral(" | ") + verifiedPatternLocalPathSnapshot,
                       fullPicPATH, d);
    }

    qWarning() << "[loopWaitPicSlot][PASS] verified SYNC compatibility fields:"
             << "PicPATHPayload=" << fullPicPATH
             << "PatternPATH=" << (fullPathPattern.isEmpty()
                                        ? QStringLiteral("<optional-empty>")
                                        : fullPathPattern);

    if (loopWaitPic) {
        loopWaitPic->stop();
    }
}

void PLCServer::sendUpdateToFPGA() {
    QDir dir2("/var/www/html/fpga_update/");

    if (dir2.exists()) {
        qDebug() << "fpga_update directory exists!";

        // List files in the directory
        QStringList fileList = dir2.entryList(QDir::Files);

        if (fileList.isEmpty()) {
            qDebug() << "No files found in the directory.";
        } else {
            qDebug() << "Files in the directory:";
            for (const QString &file : fileList) {
                if (file.endsWith(".bin", Qt::CaseInsensitive) || file.endsWith(".tar", Qt::CaseInsensitive) || file.endsWith(".tar.xz", Qt::CaseInsensitive)) {
                    // Check if the file name contains 'sigsense'
                    if (file.contains("sigsense", Qt::CaseInsensitive)) {
                        qDebug() << "SigSense-related file found: " << file << " full path" << dir2.absoluteFilePath(file);
                        QString path = "http://" + networks->ip_address + "/fpga_update/" + file;
                        //                QString path = "http://"+networks->ip_address+"/fpga_update/"+"FPGA.tar";
                        qDebug() << path;
                        QJsonDocument jsonDoc;
                        QJsonObject Param;
                        QString raw_data;
                        Param.insert("objectName", "updateFirmware");
                        Param.insert("link", path);
                        Param.insert("text", version->FPGA_version);
                        jsonDoc.setObject(Param);
                        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                        emit sendMessage(raw_data, FPGA_address);
                    }
                }
            }
        }
    } else {
        qDebug() << "fpga_update directory does not exist....";
    }
}

void PLCServer::sendUpdateToMonitor() {
    QDir dir("/var/www/html/monitor_update/");

    if (dir.exists()) {
        qDebug() << "monitor_update directory exists!";

        // List files in the directory
        QStringList fileList = dir.entryList(QDir::Files);

        if (fileList.isEmpty()) {
            qDebug() << "No files found in the directory.";
        } else {
            qDebug() << "Files in the directory:";
            for (const QString &file : fileList) {
                if (file.endsWith(".bin", Qt::CaseInsensitive) || file.endsWith(".tar", Qt::CaseInsensitive) || file.endsWith(".tar.xz", Qt::CaseInsensitive)) {
                    // Check if the file name contains 'monitor'
                    if (file.contains("monitor", Qt::CaseInsensitive)) {
                        qDebug() << "Monitor-related file found: " << file << " full path" << dir.absoluteFilePath(file);
                        QString path = "http://" + networks->ip_address + "/monitor_update/" + file;
                        qDebug() << path;
                        QJsonDocument jsonDoc;
                        QJsonObject Param;
                        QString raw_data;
                        Param.insert("objectName", "updateFirmware");
                        Param.insert("link", path);
                        Param.insert("text", version->Monitor_version);
                        jsonDoc.setObject(Param);
                        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
                        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
                            if (pClient->state() == QAbstractSocket::ConnectedState)
                                emit sendMessage(raw_data, pClient);
                            else
                                qDebug() << "Monitor_address:" << pClient->state();
                        }
                        emit sendToVNC(raw_data);
                    }
                }
            }
        }
    } else {
        qDebug() << "monitor_update directory does not exist....";
    }
}

void PLCServer::sendToFPGA(QString msg) { emit sendMessage(msg, FPGA_address); }

void PLCServer::sendMessages(QString msg, QWebSocket *wClient) { emit sendMessage(msg, wClient); }

void PLCServer::Logout() {
    // QJsonDocument jsonDoc;
    // QJsonObject Param;
    // Param.insert("objectName", "Pop-up");
    // Param.insert("msg", "Logging out");
    // Param.insert("state", true);
    // jsonDoc.setObject(Param);
    // QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    // Q_FOREACH (QWebSocket *pClient, Monitor_address) {
    //     if (pClient != Monitor_address.at(Monitor_address.size() - 1)) {
    //         if (pClient->state() == QAbstractSocket::ConnectedState)
    //             emit sendMessage(raw_data, pClient);
    //         else
    //             qDebug() << "Monitor_address:" << pClient->state();
    //     }
    // }
    // //    LogoutCounter();
    // loopLogoutCounter->start();
    // loopLogout->stop();
}

void PLCServer::LogoutCounter() {
    // int i = 0;
    // while (1) {
    //     if (i >= 30) {
    //         break;
    //     }
    //     QThread::msleep(1000);
    //     i++;
    // }
    // QJsonDocument jsonDoc;
    // QJsonObject Param;
    // Param.insert("objectName", "Pop-up");
    // Param.insert("msg", "disable");
    // Param.insert("state", false);
    // jsonDoc.setObject(Param);
    // QString raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    // Q_FOREACH (QWebSocket *pClient, Monitor_address) {
    //     if (pClient == Monitor_address.at(0)){
    //         if (pClient->state() == QAbstractSocket::ConnectedState)
    //             emit sendMessage(raw_data, pClient);
    //         else
    //             qDebug() << "Monitor_address:" << pClient->state();
    //     }
    // }
    // loopLogoutCounter->stop();
}

void PLCServer::LogoutVNC() {
    QJsonDocument jsonDoc;
    QJsonObject Param;
    QString raw_data;
    qDebug() << "PLCServer::LogoutREMOTE";
    LogoutVNCCount = 0;
    stopThread4 = false;
    while (1) {
        if (LogoutVNCCount >= 60*15*10) {
        // if (LogoutVNCCount >= 30) {
            qDebug() << "i >= 60*15*10" << LogoutVNCCount;
            break;
        }
        if(stopThread4){
            qDebug() << "stopThread4" << stopThread4;
            // qWarning() << "stopThread4" << stopThread4;
            break;
        }
        QThread::msleep(100);
        LogoutVNCCount++;
    }

    // qWarning() << "pop-up close" << stopThread4;

    Param.insert("objectName", "logouts");
    jsonDoc.setObject(Param);
    raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    Q_FOREACH (QWebSocket *pClient, Monitor_address) {
        if (pClient == Monitor_address.at(0))
            emit sendMessage(raw_data, pClient);
        else
            qDebug() << "Monitor_address:" << pClient->state();
    }


    Param.insert("objectName", "Pop-up");
    Param.insert("msg", "disable");
    Param.insert("state", false);
    jsonDoc.setObject(Param);
    raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    Q_FOREACH (QWebSocket *pClient, Monitor_address) {
        if (pClient == Monitor_address.at(0))
            emit sendMessage(raw_data, pClient);
        else
            qDebug() << "Monitor_address:" << pClient->state();
    }


    Param.insert("objectName", "Backtohome");
    // if(stopThread4){
    //     Param.insert("state", "true");
    // }
    // else
    //     Param.insert("state", "false");
    Param.insert("name", "Monitor");
    jsonDoc.setObject(Param);
    raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    // qWarning() << "raw_data" << raw_data;
    // if(!change_monitor){
    if(Monitor_address.size() > 1){
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient == Monitor_address.at(Monitor_address.size()-1))
                emit sendMessage(raw_data, pClient);
            else
                qDebug() << "Monitor_address:" << pClient->state();
        }
        // qWarning() << "send Backtohome";
    }

    // }
    // qWarning() << "out scope send Backtohome";
    // if(!stopThread4){
    //     Param.insert("objectName", "Pop-up");
    //     Param.insert("msg", "Logging out");
    //     Param.insert("state", true);
    //     jsonDoc.setObject(Param);
    //     raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    //     if (MonitorSecond_address->state() == QAbstractSocket::ConnectedState)
    //         emit sendMessage(raw_data, MonitorSecond_address);
    //     else
    //         qDebug() << "MonitorSecond_address:" << MonitorSecond_address->state();
    // }

    stopThread4 = false;
    change_monitor = false;
}

void PLCServer::LogoutCounterVNC() {
    // while(1){
        QJsonDocument jsonDoc;
        QJsonObject Param;
        QString raw_data;
        // qWarning() << "PLCServer::LogoutCounterVNC";
        LogoutCounterVNCCount = 0;
        while (1) {
            if (LogoutCounterVNCCount >= 60*15*10) {
            // if (LogoutCounterVNCCount >= 100) {
                qDebug() << "i >= 60*15*10" << LogoutCounterVNCCount;
                break;
            }
            if(stopThread5){
                // qWarning() << "LogoutCounterVNC stopThread5" << stopThread5;
                break;
            }
            QThread::msleep(100);
            LogoutCounterVNCCount++;
        }

        Param.empty();
        Param.insert("objectName", "Pop-up");
        Param.insert("msg", "disable");
        Param.insert("state", false);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        Q_FOREACH (QWebSocket *pClient, Monitor_address) {
            if (pClient == Monitor_address.at(0)) {
                if (pClient->state() == QAbstractSocket::ConnectedState){
                    emit sendMessage(raw_data, pClient);
                    qDebug() << "pClient != pClient" << raw_data << " stopThread5:" << stopThread5;
                }
                else
                    qDebug() << "Monitor_address:" << pClient->state();
            }
        }
        // qWarning() << "REMOTE disconnect";
        // if (vnc_address->state() == QAbstractSocket::ConnectedState)
        //     emit sendMessage(raw_data, vnc_address);
        // else
        //     qDebug() << "vnc_address:" << vnc_address->state();
        // Param.insert("objectName", "Pop-up");
        // Param.insert("msg", "disable");
        // Param.insert("state", false);
        // jsonDoc.setObject(Param);
        // raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        // Q_FOREACH (QWebSocket *pClient, Monitor_address) {
        //     if (vnc_address != pClient) {
        //         if (pClient->state() == QAbstractSocket::ConnectedState)
        //             emit sendMessage(raw_data, pClient);
                // else
                //     qDebug() << "Monitor_address:" << pClient->state();
        //     }
        // }

        qDebug() << "cancel animation stopThread5";
        // if(stopThread5){
        qDebug() << "cancel animation stopThread5";
        Param.empty();
        Param.insert("objectName", "Pop-up");
        Param.insert("msg", "Logging out");
        Param.insert("state", true);
        jsonDoc.setObject(Param);
        raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
        emit sendToVNC(raw_data);
        // qWarning() << "raw_data" << raw_data;
                // if (vnc_address->state() == QAbstractSocket::ConnectedState){
                //     emit sendMessage(raw_data, vnc_address);
                //     qDebug() << "vnc_address != pClient" << raw_data << " stopThread5:" << stopThread5;
                // }
                // else
                //     qDebug() << "vnc_address:" << vnc_address->state();
            // }
        // }
        // }

        // qWarning() << "REMOTE VNC disconnect";
        stopThread5 = false;
        QThread::msleep(10);
    // }
}

void PLCServer::clearFpgaUpdateBackground()
{
    removeFilesInFixedDir(QStringLiteral("/var/www/html/fpga_update"));
    qDebug() << "[clearFpgaUpdateBackground] cleanup finished";
}

void PLCServer::clearMonitorUpdateBackground()
{
    removeFilesInFixedDir(QStringLiteral("/var/www/html/monitor_update"));
    qDebug() << "[clearMonitorUpdateBackground] cleanup finished";
}

void PLCServer::link_sdcard_webSlot()
{
    const QString script = QStringLiteral("/usr/local/bin/link_sdcard_web.sh");
    const QFileInfo scriptInfo(script);

    if (!scriptInfo.exists() || !scriptInfo.isFile() || scriptInfo.isSymLink()) {
        qWarning() << "[link_sdcard_webSlot] script missing or unsafe:" << script;
        return;
    }

    if (!QProcess::startDetached(script, QStringList{})) {
        qWarning() << "[link_sdcard_webSlot] failed to start script:" << script;
        return;
    }

    qDebug() << "[link_sdcard_webSlot] script started in background:" << script;
}

void PLCServer::assignDisplayGerneral(double volt,QString sub,QString dir,QString line){
    SetupEquipment->Voltage = volt;
    SetupEquipment->SubstationName = sub;
    SetupEquipment->LFLSerialNo = line;
    QJsonDocument jsonDoc;
    QJsonObject Param;
    QString raw_data;
    Param.insert("menuID", "updateEquipmentdata");
    Param.insert("SubstationName", SetupEquipment->SubstationName);
    Param.insert("Voltage", SetupEquipment->Voltage);
    Param.insert("TransmissionLineName", SetupEquipment->TransmissionLineName);
    Param.insert("Distance", SetupEquipment->Distance);
    Param.insert("IPaddress", SetupEquipment->IPaddress);
    Param.insert("Brand", SetupEquipment->Brand);
    Param.insert("Model", SetupEquipment->Model);
    Param.insert("SerialNo", SetupEquipment->SerialNo);
    Param.insert("ContractNumber", SetupEquipment->ContractNumber);
    Param.insert("Date", SetupEquipment->Date);
    Param.insert("LFLSerialNo", SetupEquipment->LFLSerialNo);
    jsonDoc.setObject(Param);
    raw_data = QJsonDocument(Param).toJson(QJsonDocument::Compact).toStdString().c_str();
    if (snmp_address->state() == QAbstractSocket::ConnectedState) {
        emit sendMessage(raw_data, snmp_address);
        qDebug() << "updateEquipmentdata: SubstationName";
    } else
        qDebug() << "snmp_address:" << snmp_address->state();

    Q_FOREACH (QWebSocket *pClient, webapp_address) {
        emit sendMessage(raw_data, pClient);
    }
}

void PLCServer::operateTimeSlot(){
    qWarning() << "operateTimeSlot stop";
    QJsonObject Param;
    QJsonArray Reg;
    QString raw_datas;

    // // ============================================================
    // // ✅ SAFE HELPERS (กัน nullptr / pointer แขวน / state crash)
    // // ============================================================
    // auto wsConnected = [](const QWebSocket *ws) -> bool {
    //     return ws && (ws->state() == QAbstractSocket::ConnectedState);
    // };

    // auto safeSendOne = [&](const QString &raw, QWebSocket *ws, const char *tag) {
    //     if (wsConnected(ws)) {
    //         emit sendMessage(raw, ws);
    //     } else {
    //         qDebug() << tag << "not ready/disconnected ptr=" << ws
    //                  << "state=" << (ws ? ws->state() : QAbstractSocket::UnconnectedState);
    //     }
    // };

    // // ✅ helper สร้าง json compact เป็น QString แบบถูกต้อง
    // auto jsonCompact = [](const QJsonObject &obj) -> QString {
    //     return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    // };


    QJsonDocument jsonDoc;
    // QJsonObject Param;
    QString raw_data;

    if (language == 0) {
        QJsonObject P;
        P.insert("objectName", "writeCoilsOperate");
        P.insert("state", false);
        jsonDoc.setObject(P);
        raw_data = QJsonDocument(P).toJson(QJsonDocument::Compact).toStdString().c_str();
        if (INPUT_PLC_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(raw_data, INPUT_PLC_address);
            qWarning() << "writeCoilsOperate: raw_data";
        } else
            qDebug() << "INPUT_PLC_address:" << INPUT_PLC_address->state();
        // safeSendOne(raw_datas, INPUT_PLC_address, "INPUT_PLC_address");
    } else if (language == 1) {
        Reg = QJsonArray();
        Reg.append(0);
        QJsonObject P;
        P.insert("objectName", "writeCoils");
        P.insert("index", 801);
        P.insert("register", Reg);
        jsonDoc.setObject(P);
        raw_data = QJsonDocument(P).toJson(QJsonDocument::Compact).toStdString().c_str();
        if (INPUT_PLC_address->state() == QAbstractSocket::ConnectedState) {
            emit sendMessage(raw_data, INPUT_PLC_address);
            qDebug() << "writeCoilsOperate: raw_data";
        } else
            qDebug() << "INPUT_PLC_address:" << INPUT_PLC_address->state();
        // safeSendOne(raw_datas, OpenPLC_address, "OpenPLC_address");
    }

    operateTime->stop();
}

QString PLCServer::eventPictureStateName() const
{
    switch (eventPictureState) {
    case EventPictureWaiting:    return QStringLiteral("WAITING");
    case EventPictureProcessing: return QStringLiteral("PROCESSING");
    case EventPictureVerified:   return QStringLiteral("VERIFIED");
    case EventPictureClosed:     return QStringLiteral("CLOSED");
    case EventPictureIdle:
    default:                     return QStringLiteral("IDLE");
    }
}

void PLCServer::resetEventPictureOwnership(const QString &eventId,
                                           const QString &mode,
                                           const QString &eventDate,
                                           const QString &eventTime)
{
    // A new event invalidates any delayed ScreenPicture request from the
    // previous event before it can be processed under the new event context.
    if (screenPictureDelayTimer && screenPictureDelayTimer->isActive()) {
        screenPictureDelayTimer->stop();
    }

    pendingScreenLink.clear();
    pendingScreenFileName.clear();
    pendingScreenMode.clear();
    pendingScreenSenderAddress.clear();
    pendingScreenSenderPort = 0;

    activePictureEventId = eventId.trimmed();
    acceptedPictureName.clear();

    const QString cleanMode = mode.trimmed();
    const bool supportedPictureMode =
        cleanMode.compare(QStringLiteral("Manual"), Qt::CaseInsensitive) == 0 ||
        cleanMode.compare(QStringLiteral("Relay"), Qt::CaseInsensitive) == 0 ||
        cleanMode.compare(QStringLiteral("Surge"), Qt::CaseInsensitive) == 0 ||
        cleanMode.compare(QStringLiteral("Periodic"), Qt::CaseInsensitive) == 0;
    eventPictureActive = !activePictureEventId.isEmpty() && supportedPictureMode;
    eventPictureState = eventPictureActive ? EventPictureWaiting : EventPictureIdle;

    qWarning() << "[PIC-OWNER][RESET]"
               << "eventId=" << activePictureEventId
               << "mode=" << cleanMode
               << "date=" << eventDate
               << "time=" << eventTime
               << "active=" << eventPictureActive
               << "state=" << eventPictureStateName();
}

void PLCServer::releaseEventPictureReservationForRetry(const QString &pictureName,
                                                       const QString &reason)
{
    if (!eventPictureActive) {
        return;
    }

    const QString cleanName = QFileInfo(pictureName.trimmed()).fileName();
    if (eventPictureState == EventPictureProcessing &&
        (acceptedPictureName.isEmpty() || cleanName.isEmpty() ||
         acceptedPictureName.compare(cleanName, Qt::CaseInsensitive) == 0)) {
        qWarning() << "[PIC-OWNER][RETRY-READY]"
                   << "eventId=" << activePictureEventId
                   << "picture=" << acceptedPictureName
                   << "reason=" << reason;
        acceptedPictureName.clear();
        eventPictureState = EventPictureWaiting;
    }
}

void PLCServer::closeEventPictureOwnership(const QString &reason)
{
    qWarning() << "[PIC-OWNER][CLOSE]"
               << "eventId=" << activePictureEventId
               << "picture=" << acceptedPictureName
               << "stateBefore=" << eventPictureStateName()
               << "reason=" << reason;

    if (screenPictureDelayTimer && screenPictureDelayTimer->isActive()) {
        screenPictureDelayTimer->stop();
    }

    pendingScreenLink.clear();
    pendingScreenFileName.clear();
    pendingScreenMode.clear();
    pendingScreenSenderAddress.clear();
    pendingScreenSenderPort = 0;

    eventPictureActive = false;
    eventPictureState = EventPictureClosed;
}

void PLCServer::processDelayedScreenPicture()
{
    if (pendingScreenLink.isEmpty() || pendingScreenFileName.isEmpty()) {
        qWarning() << "[ScreenPicture] pending data is empty";
        releaseEventPictureReservationForRetry(acceptedPictureName,
                                               QStringLiteral("delayed ScreenPicture data became empty"));
        pendingScreenLink.clear();
        pendingScreenFileName.clear();
        pendingScreenMode.clear();
        pendingScreenSenderAddress.clear();
        pendingScreenSenderPort = 0;
        screenPictureDelayTimer->stop();
        return;
    }

    if (!eventPictureActive || eventPictureState != EventPictureProcessing ||
        acceptedPictureName.compare(QFileInfo(pendingScreenFileName).fileName(),
                                    Qt::CaseInsensitive) != 0) {
        qWarning() << "[PIC-OWNER][BLOCK-DELAYED]"
                   << "eventId=" << activePictureEventId
                   << "state=" << eventPictureStateName()
                   << "accepted=" << acceptedPictureName
                   << "pending=" << pendingScreenFileName
                   << "sender=" << pendingScreenSenderAddress << pendingScreenSenderPort;
        pendingScreenLink.clear();
        pendingScreenFileName.clear();
        pendingScreenMode.clear();
        pendingScreenSenderAddress.clear();
        pendingScreenSenderPort = 0;
        screenPictureDelayTimer->stop();
        return;
    }

    // qWarning() << "[ScreenPicture] delayed process:"
    //            << "link =" << pendingScreenLink
    //            << "fileName =" << pendingScreenFileName
    //            << "mode =" << pendingScreenMode;

    if (!open_interlock) {
        qWarning() << "[PIC-OWNER][PROCESS]"
                   << "eventId=" << activePictureEventId
                   << "picture=" << pendingScreenFileName
                   << "sender=" << pendingScreenSenderAddress << pendingScreenSenderPort;

        emit getScreenPicture(pendingScreenLink, pendingScreenFileName);

        // getScreenPictureandSave() is a direct same-thread slot in the current
        // design.  It either promotes the reservation to VERIFIED or releases
        // it back to WAITING on failure.
        if (eventPictureState == EventPictureVerified && loopWaitPic) {
            loopWaitPic->start(3000);
        } else if (eventPictureState == EventPictureProcessing) {
            releaseEventPictureReservationForRetry(
                pendingScreenFileName,
                QStringLiteral("picture transaction returned without VERIFIED state"));
        }

        sendDIO = false;
    } else {
        releaseEventPictureReservationForRetry(
            pendingScreenFileName,
            QStringLiteral("ScreenPicture suppressed by open_interlock"));
    }

    open_interlock = false;

    pendingScreenLink.clear();
    pendingScreenFileName.clear();
    pendingScreenMode.clear();
    pendingScreenSenderAddress.clear();
    pendingScreenSenderPort = 0;

    QString msg="";
    QJsonObject POPUP;
    QJsonDocument jsonDoc;
    POPUP.insert("objectName", "process_msg");
    if (testModeRec == "ManualTest") {
        msg = "The MANUAL_TEST_EVENT process is currently running, please wait for it to complete.";
    } else if (testModeRec == "Surge") {
        msg = QString("The SURGE_START_EVENT_%1 process is currently running, please wait for it to complete.").arg(chanelRec);
    } else if (testModeRec == "RelayTest") {
        msg = "The RELAY_START_EVENT process is currently running, please wait for it to complete.";
    } else if (testModeRec == "Periodic") {
        msg = "The PERIODIC_TEST_EVENT process is currently running, please wait for it to complete.";
    }
    POPUP.insert("msg", msg);
    jsonDoc.setObject(POPUP);
    QString popup_msg = QJsonDocument(POPUP).toJson(QJsonDocument::Compact).toStdString().c_str();
    qDebug() << "processDelayedScreenPicture" << popup_msg;
    emit sendToVNC(popup_msg);
    Q_FOREACH (QWebSocket *pClient, Monitor_address) {
        if (pClient->state() == QAbstractSocket::ConnectedState)
            emit sendMessage(popup_msg, pClient);
        else
            qDebug() << "Monitor_address:" << pClient->state();
    }

    screenPictureDelayTimer->stop();
}
